#pragma once

#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gungnir/scheduler/clock.hpp>
#include <gungnir/scheduler/task.hpp>

namespace gungnir::scheduler {

class Scheduler {
public:
    explicit Scheduler(Clock& clock) : clock_(&clock) {}

    Scheduler& every(std::string name, std::chrono::seconds interval, Task::Action action) {
        if (name.empty()) throw std::invalid_argument("Scheduled task name must not be empty");
        if (interval <= std::chrono::seconds::zero()) {
            throw std::invalid_argument("Scheduled task interval must be positive");
        }
        if (!names_.insert(name).second) {
            throw std::logic_error("Scheduled task is already registered: " + name);
        }
        tasks_.emplace_back(std::move(name), interval, std::move(action));
        return *this;
    }

    [[nodiscard]] std::size_t run_due() {
        const auto now = clock_->now();
        std::size_t count = 0;
        for (auto& task : tasks_) {
            if (!task.due(now)) continue;
            task.run(now);
            ++count;
        }
        return count;
    }

    [[nodiscard]] std::size_t size() const noexcept { return tasks_.size(); }

private:
    Clock* clock_;
    std::unordered_set<std::string> names_;
    std::vector<Task> tasks_;
};

} // namespace gungnir::scheduler
