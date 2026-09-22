#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <utility>

#include <gungnir/scheduler/clock.hpp>

namespace gungnir::scheduler {

class Task {
public:
    using Action = std::function<void()>;

    Task(std::string name, std::chrono::seconds interval, Action action)
        : name_(std::move(name)), interval_(interval), action_(std::move(action)) {}

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] std::chrono::seconds interval() const noexcept { return interval_; }

    [[nodiscard]] bool due(Clock::TimePoint now) const noexcept {
        return !has_run_ || now - last_run_ >= interval_;
    }

    void run(Clock::TimePoint now) {
        action_();
        last_run_ = now;
        has_run_ = true;
    }

private:
    std::string name_;
    std::chrono::seconds interval_;
    Action action_;
    Clock::TimePoint last_run_{};
    bool has_run_{false};
};

} // namespace gungnir::scheduler
