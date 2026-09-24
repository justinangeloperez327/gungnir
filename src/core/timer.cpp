#include <gungnir/core/timer.hpp>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <gungnir/core/executor.hpp>

namespace gungnir {

namespace {

class TimerScheduler {
public:
    TimerScheduler()
        : executor_{} {
        executor_.start();

        timer_thread_ =
            std::thread{
                [this] {
                    run();
                }
            };
    }

    ~TimerScheduler() {
        {
            std::lock_guard lock{
                mutex_
            };

            stopping_ = true;
        }

        ready_.notify_all();

        if (
            timer_thread_.joinable()
        ) {
            timer_thread_.join();
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
        std::coroutine_handle<> handle
    ) {
        if (!handle) {
            return;
        }

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
                        handle
                }
            );
        }

        ready_.notify_one();
    }

private:
    using Clock =
        std::chrono::steady_clock;

    struct Entry {
        Clock::time_point deadline;
        std::uint64_t sequence;
        std::coroutine_handle<> handle;
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

    void run() noexcept {
        std::unique_lock lock{
            mutex_
        };

        while (true) {
            if (stopping_) {
                return;
            }

            if (timers_.empty()) {
                ready_.wait(
                    lock,
                    [this] {
                        return
                            stopping_ ||
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

            std::vector<
                std::coroutine_handle<>
            > due;

            const auto now =
                Clock::now();

            while (
                !timers_.empty() &&
                timers_.top()
                    .deadline <= now
            ) {
                due.push_back(
                    timers_.top()
                        .handle
                );

                timers_.pop();
            }

            lock.unlock();

            for (
                const auto handle :
                due
            ) {
                if (
                    !handle ||
                    handle.done()
                ) {
                    continue;
                }

                try {
                    executor_.schedule(
                        handle
                    );
                } catch (...) {
                    if (
                        handle &&
                        !handle.done()
                    ) {
                        handle.resume();
                    }
                }
            }

            lock.lock();
        }
    }

    Executor executor_;
    std::thread timer_thread_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::priority_queue<
        Entry,
        std::vector<Entry>,
        Later
    > timers_;
    std::uint64_t sequence_{0};
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
        handle
    );
}

} // namespace gungnir
