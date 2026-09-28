#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include <gungnir/scheduler/clock.hpp>
#include <gungnir/scheduler/cron.hpp>

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
        Action action
    )
        : name_(
            std::move(name)
          ),
          cron_(
            std::move(cron)
          ),
          action_(
            std::move(action)
          ) {}

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

        const auto slot =
            std::chrono::floor<
                std::chrono::minutes
            >(now);

        return
            cron_->matches_utc(now) &&
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

        const auto slot =
            std::chrono::floor<
                std::chrono::minutes
            >(now);

        if (
            cron_->matches_utc(now) &&
            (
                !last_cron_slot_ ||
                *last_cron_slot_ != slot
            )
        ) {
            return now;
        }

        return
            cron_->next_after_utc(
                slot
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
                std::chrono::floor<
                    std::chrono::minutes
                >(now);
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
    std::optional<
        std::chrono::sys_time<
            std::chrono::minutes
        >
    > last_cron_slot_;
    bool has_run_{false};
};

} // namespace gungnir::scheduler
