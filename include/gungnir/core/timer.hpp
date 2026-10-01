#pragma once

#include <chrono>
#include <gungnir/core/cancellation.hpp>
#include <gungnir/core/resume_slot.hpp>
#include <coroutine>
#include <cstdint>

namespace gungnir {

class SleepAwaiter {
public:
    explicit SleepAwaiter(
        std::chrono::milliseconds duration, CancellationToken cancellation = {}
    ) noexcept
        : duration_(duration), cancellation_(std::move(cancellation)) {}
    SleepAwaiter(SleepAwaiter&&) noexcept = default;
    ~SleepAwaiter() { if (slot_) slot_->cancel(); }

    [[nodiscard]]
    bool await_ready()
        const noexcept {
        return duration_.count() <= 0 || cancellation_.cancelled();
    }

    void await_suspend(
        std::coroutine_handle<> handle
    );

    void await_resume()
        const { cancellation_.throw_if_cancelled(); }

private:
    std::chrono::milliseconds duration_;
    CancellationToken cancellation_;
    std::shared_ptr<detail::ResumeSlot> slot_;
    std::shared_ptr<CancellationRegistration> registration_;
};

[[nodiscard]]
inline SleepAwaiter sleep_for(
    std::chrono::milliseconds duration, CancellationToken cancellation = {}
) noexcept {
    return SleepAwaiter{
        duration, std::move(cancellation)
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
