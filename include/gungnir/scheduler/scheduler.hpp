#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gungnir/scheduler/clock.hpp>
#include <gungnir/scheduler/cron.hpp>
#include <gungnir/scheduler/task.hpp>

namespace gungnir::scheduler {

class Scheduler {
public:
    explicit Scheduler(
        Clock& clock
    )
        : clock_(&clock) {}

    Task& every(
        std::string name,
        std::chrono::seconds interval,
        Task::Action action
    ) {
        validate_name(name);

        if (
            interval <=
            std::chrono::seconds::zero()
        ) {
            throw std::invalid_argument(
                "Scheduled task interval must be positive"
            );
        }

        tasks_.emplace_back(
            std::move(name),
            interval,
            std::move(action)
        );

        return tasks_.back();
    }

    Task& cron(
        std::string name,
        std::string expression,
        Task::Action action
    ) {
        validate_name(name);

        tasks_.emplace_back(
            std::move(name),
            CronExpression{
                std::move(expression)
            },
            std::move(action)
        );

        return tasks_.back();
    }

    Task& hourly(
        std::string name,
        Task::Action action
    ) {
        return cron(
            std::move(name),
            "0 * * * *",
            std::move(action)
        );
    }

    Task& daily(
        std::string name,
        Task::Action action
    ) {
        return cron(
            std::move(name),
            "0 0 * * *",
            std::move(action)
        );
    }

    Task& weekly(
        std::string name,
        Task::Action action
    ) {
        return cron(
            std::move(name),
            "0 0 * * 0",
            std::move(action)
        );
    }

    Task& monthly(
        std::string name,
        Task::Action action
    ) {
        return cron(
            std::move(name),
            "0 0 1 * *",
            std::move(action)
        );
    }

    [[nodiscard]]
    std::size_t run_due() {
        const auto now =
            clock_->now();

        std::size_t count = 0;

        for (auto& task : tasks_) {
            if (!task.due(now)) {
                continue;
            }

            task.run(now);
            ++count;
        }

        return count;
    }

    [[nodiscard]]
    Clock::TimePoint next_due()
        const {
        const auto now =
            clock_->now();

        auto next =
            Clock::TimePoint::max();

        for (const auto& task : tasks_) {
            next =
                std::min(
                    next,
                    task.next_due(now)
                );
        }

        return next;
    }

    [[nodiscard]]
    std::size_t size()
        const noexcept {
        return tasks_.size();
    }

private:
    void validate_name(
        const std::string& name
    ) {
        if (name.empty()) {
            throw std::invalid_argument(
                "Scheduled task name must not be empty"
            );
        }

        if (
            !names_
                .insert(name)
                .second
        ) {
            throw std::logic_error(
                "Scheduled task is already registered: " +
                name
            );
        }
    }

    Clock* clock_;
    std::unordered_set<
        std::string
    > names_;
    std::vector<Task> tasks_;
};

} // namespace gungnir::scheduler
