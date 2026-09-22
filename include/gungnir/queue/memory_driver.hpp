#pragma once

#include <deque>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include <gungnir/queue/driver.hpp>

namespace gungnir::queue {

class MemoryDriver final : public Driver {
public:
    void push(Envelope job) override {
        std::lock_guard lock{mutex_};
        pending_.push_back(std::move(job));
    }

    [[nodiscard]] std::optional<Envelope> pop() override {
        std::lock_guard lock{mutex_};
        if (pending_.empty()) return std::nullopt;
        auto job = std::move(pending_.front());
        pending_.pop_front();
        return job;
    }

    void acknowledge(const Envelope&) override {}

    void release(Envelope job) override {
        std::lock_guard lock{mutex_};
        pending_.push_back(std::move(job));
    }

    void fail(const Envelope& job) override {
        std::lock_guard lock{mutex_};
        failed_.push_back(job);
    }

    [[nodiscard]] std::size_t pending() const {
        std::lock_guard lock{mutex_};
        return pending_.size();
    }

    [[nodiscard]] std::size_t failed() const {
        std::lock_guard lock{mutex_};
        return failed_.size();
    }

private:
    mutable std::mutex mutex_;
    std::deque<Envelope> pending_;
    std::vector<Envelope> failed_;
};

} // namespace gungnir::queue
