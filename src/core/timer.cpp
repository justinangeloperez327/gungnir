#include <gungnir/core/timer.hpp>

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <gungnir/core/executor.hpp>
#include <gungnir/observability/trace.hpp>

namespace gungnir {

namespace {

class TimerScheduler {
public:
    TimerScheduler()
        : executor_{} {
        executor_.start();

        fallback_thread_ =
            std::thread{
                [this] {
                    run_fallback();
                }
            };
    }

    ~TimerScheduler() {
        {
            std::lock_guard lock{
                mutex_
            };

            stopping_ = true;
            pump_token_ = 0;
            pump_context_ = nullptr;
            pump_wake_ = nullptr;
        }

        ready_.notify_all();

        if (
            fallback_thread_.joinable()
        ) {
            fallback_thread_.join();
        }

        executor_.stop();
        executor_.join();
    }

    TimerScheduler(
        const TimerScheduler&
    ) = delete;

    TimerScheduler& operator=(
        const TimerScheduler&
    ) = delete;

    void schedule(
        std::chrono::milliseconds duration,
        std::coroutine_handle<> handle,
        observability::TraceContext trace_context
    ) {
        if (!handle) {
            return;
        }

        detail::TimerWakeFunction wake =
            nullptr;

        void* wake_context = nullptr;

        {
            std::lock_guard lock{
                mutex_
            };

            if (stopping_) {
                throw std::logic_error(
                    "Gungnir timer scheduler is stopping"
                );
            }

            timers_.push(
                Entry{
                    .deadline =
                        Clock::now() +
                        duration,
                    .sequence =
                        sequence_++,
                    .handle =
                        handle,
                    .context =
                        std::move(
                            trace_context
                        )
                }
            );

            if (pump_token_ != 0) {
                wake = pump_wake_;
                wake_context =
                    pump_context_;
            }
        }

        if (wake != nullptr) {
            wake(wake_context);
        } else {
            ready_.notify_one();
        }
    }

    std::uint64_t attach(
        void* context,
        detail::TimerWakeFunction wake
    ) {
        if (wake == nullptr) {
            throw std::invalid_argument(
                "Timer pump wake callback is required"
            );
        }

        std::uint64_t token = 0;

        {
            std::lock_guard lock{
                mutex_
            };

            if (stopping_) {
                throw std::logic_error(
                    "Gungnir timer scheduler is stopping"
                );
            }

            if (pump_token_ != 0) {
                throw std::logic_error(
                    "A Gungnir timer pump is already attached"
                );
            }

            token =
                next_pump_token_++;

            if (token == 0) {
                token =
                    next_pump_token_++;
            }

            pump_token_ = token;
            pump_context_ = context;
            pump_wake_ = wake;
        }

        ready_.notify_all();

        wake(context);

        return token;
    }

    void detach(
        std::uint64_t token
    ) noexcept {
        if (token == 0) {
            return;
        }

        bool detached = false;

        {
            std::lock_guard lock{
                mutex_
            };

            if (
                pump_token_ ==
                token
            ) {
                pump_token_ = 0;
                pump_context_ = nullptr;
                pump_wake_ = nullptr;
                detached = true;
            }
        }

        if (detached) {
            ready_.notify_all();
        }
    }

    [[nodiscard]]
    std::chrono::milliseconds
    poll_timeout(
        std::chrono::milliseconds maximum
    ) {
        if (
            maximum.count() <= 0
        ) {
            return
                std::chrono::milliseconds{
                    0
                };
        }

        std::lock_guard lock{
            mutex_
        };

        if (
            pump_token_ == 0 ||
            timers_.empty()
        ) {
            return maximum;
        }

        const auto now =
            Clock::now();

        const auto deadline =
            timers_.top()
                .deadline;

        if (deadline <= now) {
            return
                std::chrono::milliseconds{
                    0
                };
        }

        const auto remaining =
            deadline - now;

        auto rounded =
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(remaining);

        if (
            rounded <
            remaining
        ) {
            rounded +=
                std::chrono::milliseconds{
                    1
                };
        }

        return std::min(
            maximum,
            rounded
        );
    }

    void dispatch_due() noexcept {
        std::vector<Entry> due;

        {
            std::lock_guard lock{
                mutex_
            };

            if (
                pump_token_ == 0
            ) {
                return;
            }

            collect_due_locked(
                due,
                Clock::now()
            );
        }

        dispatch(
            due
        );
    }

private:
    using Clock =
        std::chrono::steady_clock;

    struct Entry {
        Clock::time_point deadline;
        std::uint64_t sequence;
        std::coroutine_handle<> handle;
        observability::TraceContext
            context;
    };

    struct Later {
        [[nodiscard]]
        bool operator()(
            const Entry& left,
            const Entry& right
        ) const noexcept {
            if (
                left.deadline ==
                right.deadline
            ) {
                return
                    left.sequence >
                    right.sequence;
            }

            return
                left.deadline >
                right.deadline;
        }
    };

    void collect_due_locked(
        std::vector<Entry>& due,
        Clock::time_point now
    ) {
        while (
            !timers_.empty() &&
            timers_.top()
                .deadline <= now
        ) {
            due.push_back(
                timers_.top()
            );

            timers_.pop();
        }
    }

    void dispatch(
        const std::vector<Entry>& due
    ) noexcept {
        for (
            const auto& entry :
            due
        ) {
            if (
                !entry.handle ||
                entry.handle.done()
            ) {
                continue;
            }

            auto scope =
                observability::activate(
                    entry.context
                );

            try {
                executor_.schedule(
                    entry.handle
                );
            } catch (...) {
                if (
                    entry.handle &&
                    !entry.handle.done()
                ) {
                    entry.handle.resume();
                }
            }
        }
    }

    void run_fallback() noexcept {
        std::unique_lock lock{
            mutex_
        };

        while (true) {
            if (stopping_) {
                return;
            }

            if (pump_token_ != 0) {
                ready_.wait(
                    lock,
                    [this] {
                        return
                            stopping_ ||
                            pump_token_ == 0;
                    }
                );

                continue;
            }

            if (timers_.empty()) {
                ready_.wait(
                    lock,
                    [this] {
                        return
                            stopping_ ||
                            pump_token_ != 0 ||
                            !timers_.empty();
                    }
                );

                continue;
            }

            const auto deadline =
                timers_.top()
                    .deadline;

            if (
                Clock::now() <
                deadline
            ) {
                ready_.wait_until(
                    lock,
                    deadline
                );

                continue;
            }

            std::vector<Entry> due;

            collect_due_locked(
                due,
                Clock::now()
            );

            lock.unlock();

            dispatch(
                due
            );

            lock.lock();
        }
    }

    Executor executor_;
    std::thread fallback_thread_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::priority_queue<
        Entry,
        std::vector<Entry>,
        Later
    > timers_;
    std::uint64_t sequence_{0};
    std::uint64_t next_pump_token_{1};
    std::uint64_t pump_token_{0};
    void* pump_context_{nullptr};
    detail::TimerWakeFunction pump_wake_{
        nullptr
    };
    bool stopping_{false};
};

TimerScheduler& timer_scheduler() {
    static TimerScheduler scheduler;

    return scheduler;
}

} // namespace

void SleepAwaiter::await_suspend(
    std::coroutine_handle<> handle
) const {
    timer_scheduler().schedule(
        duration_,
        handle,
        observability::
            current_context()
    );

    observability::
        clear_current_context();
}

namespace detail {

std::uint64_t attach_timer_pump(
    void* context,
    TimerWakeFunction wake
) {
    return timer_scheduler().attach(
        context,
        wake
    );
}

void detach_timer_pump(
    std::uint64_t token
) noexcept {
    timer_scheduler().detach(
        token
    );
}

std::chrono::milliseconds
timer_poll_timeout(
    std::chrono::milliseconds maximum
) {
    return
        timer_scheduler().poll_timeout(
            maximum
        );
}

void dispatch_due_timers() noexcept {
    timer_scheduler().dispatch_due();
}

} // namespace detail

} // namespace gungnir
