#pragma once
#include <chrono>
#include <coroutine>
#include <thread>

namespace gungnir {

class SleepAwaiter {
public:
    explicit SleepAwaiter(std::chrono::milliseconds duration) : duration_(duration) {}
    bool await_ready() const noexcept { return duration_.count() <= 0; }
    void await_suspend(std::coroutine_handle<> handle) const {
        std::thread([duration = duration_, handle] {
            std::this_thread::sleep_for(duration);
            if (handle && !handle.done()) handle.resume();
        }).detach();
    }
    void await_resume() const noexcept {}
private:
    std::chrono::milliseconds duration_;
};

[[nodiscard]] inline SleepAwaiter sleep_for(std::chrono::milliseconds duration) { return SleepAwaiter{duration}; }

} // namespace gungnir
