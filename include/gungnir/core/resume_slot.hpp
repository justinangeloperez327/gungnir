#pragma once
#include <coroutine>
#include <memory>
#include <mutex>
#include <utility>

namespace gungnir::detail {
// An awaiter invalidates its queued continuation before its frame disappears.
class ResumeSlot {
public:
    explicit ResumeSlot(std::coroutine_handle<> handle) : handle_(handle) {}
    void resume() {
        std::lock_guard lock{mutex_};
        if (auto handle = std::exchange(handle_, {})) handle.resume();
    }
    void cancel() noexcept { std::lock_guard lock{mutex_}; handle_ = {}; }
private:
    std::recursive_mutex mutex_;
    std::coroutine_handle<> handle_;
};
}
