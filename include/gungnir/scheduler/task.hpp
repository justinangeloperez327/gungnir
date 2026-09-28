#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include <gungnir/observability/trace.hpp>
#include <gungnir/scheduler/clock.hpp>
#include <gungnir/scheduler/cron.hpp>
#include <gungnir/scheduler/lock.hpp>
#include <gungnir/scheduler/timezone.hpp>

namespace gungnir::scheduler {

class Task {
public:
    using Action = std::function<void()>;

    Task(
        std::string name,
        std::chrono::milliseconds interval,
        Action action
    )
        : name_(
            std::move(name)
          ),
          interval_(interval),
          action_(
            std::move(action)
          ) {}

    Task(
        std::string name,
        CronExpression cron,
        Action action,
        const TimeZone& timezone =
            UtcTimeZone::instance()
    )
        : name_(
            std::move(name)
          ),
          cron_(
            std::move(cron)
          ),
          action_(
            std::move(action)
          ),
          timezone_(&timezone) {}

    [[nodiscard]]
    const std::string& name()
        const noexcept {
        return name_;
    }

    [[nodiscard]]
    bool is_interval()
        const noexcept {
        return interval_.has_value();
    }

    [[nodiscard]]
    bool is_cron()
        const noexcept {
        return cron_.has_value();
    }

    [[nodiscard]]
    std::optional<
        std::chrono::milliseconds
    > interval()
        const noexcept {
        return interval_;
    }

    [[nodiscard]]
    const CronExpression*
    cron_expression()
        const noexcept {
        if (!cron_) {
            return nullptr;
        }

        return &*cron_;
    }

    Task& timezone(
        const TimeZone& timezone
    ) noexcept {
        timezone_ = &timezone;
        return *this;
    }

    [[nodiscard]]
    const TimeZone& timezone()
        const noexcept {
        return *timezone_;
    }

    Task& without_overlapping(
        std::chrono::milliseconds ttl =
            std::chrono::hours{24}
    ) {
        validate_lock_ttl(ttl);
        prevent_overlap_ = true;
        overlap_ttl_ = ttl;
        return *this;
    }

    Task& on_one_server(
        std::chrono::milliseconds ttl =
            std::chrono::hours{24}
    ) {
        validate_lock_ttl(ttl);
        one_server_ = true;
        one_server_ttl_ = ttl;
        return *this;
    }

    [[nodiscard]]
    bool prevents_overlap()
        const noexcept {
        return prevent_overlap_;
    }

    [[nodiscard]]
    bool one_server()
        const noexcept {
        return one_server_;
    }

    [[nodiscard]]
    bool due(
        Clock::TimePoint now
    ) const {
        if (interval_) {
            return
                !has_run_ ||
                now - last_run_ >=
                    *interval_;
        }

        if (!cron_) {
            return false;
        }

        const auto local =
            timezone_->to_local(now);

        const auto slot =
            wall_minute(local);

        return
            cron_->matches(local) &&
            (
                !last_cron_slot_ ||
                *last_cron_slot_ != slot
            );
    }

    [[nodiscard]]
    Clock::TimePoint next_due(
        Clock::TimePoint now
    ) const {
        if (interval_) {
            if (!has_run_) {
                return now;
            }

            return
                last_run_ +
                *interval_;
        }

        if (!cron_) {
            return
                Clock::TimePoint::max();
        }

        const auto local =
            timezone_->to_local(now);

        const auto slot =
            wall_minute(local);

        if (
            cron_->matches(local) &&
            (
                !last_cron_slot_ ||
                *last_cron_slot_ != slot
            )
        ) {
            return now;
        }

        return
            cron_->next_after(
                now,
                *timezone_,
                last_cron_slot_
            );
    }

    void run(
        Clock::TimePoint now
    ) {
        invoke_action();
        mark_observed(now);
    }

    [[nodiscard]]
    bool execute(
        Clock::TimePoint now,
        LockStore* locks
    ) {
        if (
            !prevent_overlap_ &&
            !one_server_
        ) {
            run(now);
            return true;
        }

        if (locks == nullptr) {
            throw std::logic_error(
                "Scheduled task requires a configured lock store: " +
                name_
            );
        }

        std::optional<LockLease>
            occurrence_lease;

        if (one_server_) {
            occurrence_lease =
                locks->acquire(
                    occurrence_lock_key(
                        now
                    ),
                    one_server_ttl_
                );

            if (!occurrence_lease) {
                mark_observed(now);
                return false;
            }
        }

        std::optional<LockLease>
            overlap_lease;

        if (prevent_overlap_) {
            overlap_lease =
                locks->acquire(
                    overlap_lock_key(),
                    overlap_ttl_
                );

            if (!overlap_lease) {
                mark_observed(now);
                return false;
            }
        }

        try {
            invoke_action();
            mark_observed(now);
        } catch (...) {
            if (overlap_lease) {
                static_cast<void>(
                    locks->release(
                        *overlap_lease
                    )
                );
            }

            throw;
        }

        if (overlap_lease) {
            static_cast<void>(
                locks->release(
                    *overlap_lease
                )
            );
        }

        return true;
    }

private:
    void invoke_action() {
        auto span =
            observability::
                global_tracer()
                ->start_span(
                    "scheduler.task",
                    {
                        {
                            "scheduler.task.name",
                            name_
                        },
                        {
                            "scheduler.schedule.type",
                            cron_
                                ? "cron"
                                : "interval"
                        },
                        {
                            "scheduler.timezone",
                            timezone_->name()
                        }
                    }
                );

        auto scope =
            span.valid()
                ? span.scope()
                : observability::Scope{};

        try {
            action_();

            span.status(
                observability::
                    SpanStatus::ok
            );

            span.end();
        } catch (
            const std::exception& error
        ) {
            span.error(
                error.what()
            );

            span.end();
            throw;
        } catch (...) {
            span.error(
                "Unknown scheduler task exception"
            );

            span.end();
            throw;
        }
    }

    static void validate_lock_ttl(
        std::chrono::milliseconds ttl
    ) {
        if (ttl.count() <= 0) {
            throw std::invalid_argument(
                "Scheduled task lock TTL must be greater than zero"
            );
        }
    }

    void mark_observed(
        Clock::TimePoint now
    ) {
        last_run_ = now;
        has_run_ = true;

        if (cron_) {
            last_cron_slot_ =
                wall_minute(
                    timezone_->to_local(
                        now
                    )
                );
        }
    }

    [[nodiscard]]
    std::string overlap_lock_key()
        const {
        return
            "gungnir:scheduler:overlap:" +
            name_;
    }

    [[nodiscard]]
    std::string occurrence_lock_key(
        Clock::TimePoint now
    ) const {
        std::string slot;

        if (cron_) {
            const auto local =
                timezone_->to_local(
                    now
                );

            slot =
                std::to_string(
                    local.year
                ) + "-" +
                std::to_string(
                    local.month
                ) + "-" +
                std::to_string(
                    local.day
                ) + "-" +
                std::to_string(
                    local.hour
                ) + "-" +
                std::to_string(
                    local.minute
                );
        } else if (interval_) {
            const auto milliseconds =
                std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(
                    now.time_since_epoch()
                ).count();

            const auto width =
                interval_->count();

            slot =
                std::to_string(
                    width > 0
                        ? milliseconds / width
                        : milliseconds
                );
        } else {
            slot =
                std::to_string(
                    std::chrono::
                        duration_cast<
                            std::chrono::seconds
                        >(
                            now.time_since_epoch()
                        ).count()
                );
        }

        return
            "gungnir:scheduler:single:" +
            name_ +
            ":" +
            slot;
    }

    std::string name_;
    std::optional<
        std::chrono::milliseconds
    > interval_;
    std::optional<
        CronExpression
    > cron_;
    Action action_;
    Clock::TimePoint last_run_{};
    const TimeZone* timezone_{
        &UtcTimeZone::instance()
    };
    std::optional<
        WallMinute
    > last_cron_slot_;
    bool prevent_overlap_{false};
    bool one_server_{false};
    std::chrono::milliseconds
        overlap_ttl_{
            std::chrono::hours{24}
        };
    std::chrono::milliseconds
        one_server_ttl_{
            std::chrono::hours{24}
        };
    bool has_run_{false};
};

} // namespace gungnir::scheduler
