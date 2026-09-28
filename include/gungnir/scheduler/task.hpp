#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include <gungnir/scheduler/clock.hpp>
#include <gungnir/scheduler/cron.hpp>
#include <gungnir/scheduler/timezone.hpp>

namespace gungnir::scheduler {

class Task {
public:
    using Action = std::function<void()>;

    Task(
        std::string name,
        std::chrono::seconds interval,
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
        std::chrono::seconds
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
        action_();

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

private:
    std::string name_;
    std::optional<
        std::chrono::seconds
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
    bool has_run_{false};
};

} // namespace gungnir::scheduler
