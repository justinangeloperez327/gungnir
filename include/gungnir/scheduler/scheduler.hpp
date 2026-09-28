#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/scheduler/clock.hpp>
#include <gungnir/scheduler/cron.hpp>
#include <gungnir/scheduler/lock.hpp>
#include <gungnir/scheduler/task.hpp>
#include <gungnir/scheduler/timezone.hpp>

namespace gungnir::scheduler {

struct RunnerOptions {
    std::chrono::milliseconds
        maximum_sleep{
            std::chrono::seconds{60}
        };
};

class Scheduler {
public:
    explicit Scheduler(
        Clock& clock,
        const TimeZone& timezone =
            UtcTimeZone::instance()
    )
        : clock_(&clock),
          timezone_(&timezone) {}

    Scheduler& timezone(
        const TimeZone& timezone
    ) {
        ensure_not_running();
        timezone_ = &timezone;
        return *this;
    }

    [[nodiscard]]
    const TimeZone& timezone()
        const noexcept {
        return *timezone_;
    }

    Scheduler& locks(
        LockStore& locks
    ) {
        ensure_not_running();
        locks_ = &locks;
        return *this;
    }

    [[nodiscard]]
    LockStore* lock_store()
        const noexcept {
        return locks_;
    }

    Task& every(
        std::string name,
        std::chrono::milliseconds interval,
        Task::Action action
    ) {
        validate_name(name);

        if (
            interval <=
            std::chrono::milliseconds::zero()
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
            std::move(action),
            *timezone_
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
        ensure_not_running();

        return run_due_until(
            [] {
                return false;
            }
        );
    }

    [[nodiscard]]
    std::size_t run(
        RunnerOptions options = {},
        CancellationToken cancellation = {}
    ) {
        validate_runner_options(
            options
        );

        bool expected = false;

        if (
            !running_
                .compare_exchange_strong(
                    expected,
                    true,
                    std::memory_order_acq_rel
                )
        ) {
            throw std::logic_error(
                "Scheduler runner is already running"
            );
        }

        const auto registration =
            cancellation.on_cancel(
                [this] {
                    wake();
                }
            );

        static_cast<void>(
            registration
        );

        std::size_t executed = 0;

        try {
            while (
                !stop_requested() &&
                !cancellation.cancelled()
            ) {
                executed +=
                    run_due_until(
                        [this, &cancellation] {
                            return
                                stop_requested() ||
                                cancellation.cancelled();
                        }
                    );

                if (
                    stop_requested() ||
                    cancellation.cancelled()
                ) {
                    break;
                }

                const auto now =
                    clock_->now();

                const auto deadline =
                    next_due_at(now);

                auto delay =
                    options.maximum_sleep;

                if (
                    deadline !=
                        Clock::TimePoint::max() &&
                    deadline > now
                ) {
                    const auto until =
                        std::chrono::ceil<
                            std::chrono::milliseconds
                        >(
                            deadline - now
                        );

                    delay =
                        std::min(
                            options.maximum_sleep,
                            std::max(
                                std::chrono::
                                    milliseconds{1},
                                until
                            )
                        );
                } else if (
                    deadline !=
                    Clock::TimePoint::max()
                ) {
                    delay =
                        std::chrono::
                            milliseconds{1};
                }

                std::unique_lock lock{
                    runner_mutex_
                };

                runner_ready_.wait_for(
                    lock,
                    delay,
                    [this, &cancellation] {
                        return
                            stop_requested() ||
                            cancellation.cancelled();
                    }
                );
            }
        } catch (...) {
            running_.store(
                false,
                std::memory_order_release
            );

            wake();
            throw;
        }

        running_.store(
            false,
            std::memory_order_release
        );

        wake();

        return executed;
    }

    void request_stop()
        noexcept {
        stop_requested_.store(
            true,
            std::memory_order_release
        );

        wake();
    }

    void reset_stop()
        noexcept {
        stop_requested_.store(
            false,
            std::memory_order_release
        );
    }

    [[nodiscard]]
    bool stop_requested()
        const noexcept {
        return
            stop_requested_.load(
                std::memory_order_acquire
            );
    }

    [[nodiscard]]
    bool running()
        const noexcept {
        return
            running_.load(
                std::memory_order_acquire
            );
    }

    [[nodiscard]]
    Clock::TimePoint next_due()
        const {
        if (running()) {
            throw std::logic_error(
                "next_due() cannot inspect mutable task state while the scheduler runner is active"
            );
        }

        return next_due_at(
            clock_->now()
        );
    }

    [[nodiscard]]
    std::size_t size()
        const noexcept {
        return tasks_.size();
    }

private:
    template <
        typename StopPredicate
    >
    [[nodiscard]]
    std::size_t run_due_until(
        StopPredicate&& stopping
    ) {
        const auto now =
            clock_->now();

        std::size_t count = 0;

        for (auto& task : tasks_) {
            if (stopping()) {
                break;
            }

            if (!task.due(now)) {
                continue;
            }

            if (
                task.execute(
                    now,
                    locks_
                )
            ) {
                ++count;
            }
        }

        return count;
    }

    [[nodiscard]]
    Clock::TimePoint next_due_at(
        Clock::TimePoint now
    ) const {
        auto next =
            Clock::TimePoint::max();

        for (
            const auto& task :
            tasks_
        ) {
            next =
                std::min(
                    next,
                    task.next_due(now)
                );
        }

        return next;
    }

    void validate_name(
        const std::string& name
    ) {
        ensure_not_running();

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

    static void validate_runner_options(
        const RunnerOptions& options
    ) {
        if (
            options
                .maximum_sleep
                .count() <= 0
        ) {
            throw std::invalid_argument(
                "Scheduler maximum_sleep must be greater than zero"
            );
        }
    }

    void ensure_not_running()
        const {
        if (running()) {
            throw std::logic_error(
                "Scheduler configuration cannot change while the runner is active"
            );
        }
    }

    void wake()
        noexcept {
        runner_ready_.notify_all();
    }

    Clock* clock_;
    const TimeZone* timezone_;
    LockStore* locks_{nullptr};
    std::unordered_set<
        std::string
    > names_;
    std::deque<Task> tasks_;
    std::atomic_bool stop_requested_{
        false
    };
    std::atomic_bool running_{
        false
    };
    mutable std::mutex runner_mutex_;
    std::condition_variable runner_ready_;
};

} // namespace gungnir::scheduler
