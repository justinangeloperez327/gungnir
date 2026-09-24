#pragma once

#include <chrono>
#include <coroutine>

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

} // namespace gungnir
