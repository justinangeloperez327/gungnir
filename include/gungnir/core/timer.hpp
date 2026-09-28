#pragma once

#include <chrono>
#include <coroutine>
#include <cstdint>

namespace gungnir {

class SleepAwaiter {
public:
    explicit SleepAwaiter(
        std::chrono::milliseconds duration
    ) noexcept
        : duration_(duration) {}

    [[nodiscard]]
    bool await_ready()
        const noexcept {
        return duration_.count() <= 0;
    }

    void await_suspend(
        std::coroutine_handle<> handle
    ) const;

    void await_resume()
        const noexcept {}

private:
    std::chrono::milliseconds duration_;
};

[[nodiscard]]
inline SleepAwaiter sleep_for(
    std::chrono::milliseconds duration
) noexcept {
    return SleepAwaiter{
        duration
    };
}

namespace detail {

using TimerWakeFunction =
    void (*)(void*) noexcept;

[[nodiscard]]
std::uint64_t attach_timer_pump(
    void* context,
    TimerWakeFunction wake
);

void detach_timer_pump(
    std::uint64_t token
) noexcept;

[[nodiscard]]
std::chrono::milliseconds
timer_poll_timeout(
    std::chrono::milliseconds maximum
);

void dispatch_due_timers() noexcept;

} // namespace detail

} // namespace gungnir
