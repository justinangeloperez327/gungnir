#pragma once
#include <cstddef>
#include <mutex>

namespace gungnir {

class Backpressure {
public:
    explicit Backpressure(std::size_t limit) : limit_(limit) {}
    [[nodiscard]] bool try_acquire() noexcept {
        std::lock_guard lock{mutex_};
        if (active_ >= limit_) return false;
        ++active_; return true;
    }
    void release() noexcept { std::lock_guard lock{mutex_}; if (active_ > 0) --active_; }
    [[nodiscard]] std::size_t active() const noexcept { std::lock_guard lock{mutex_}; return active_; }
    [[nodiscard]] std::size_t limit() const noexcept { return limit_; }
private:
    std::size_t limit_;
    mutable std::mutex mutex_;
    std::size_t active_{0};
};

} // namespace gungnir
