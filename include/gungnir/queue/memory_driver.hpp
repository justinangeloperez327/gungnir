#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include <gungnir/queue/driver.hpp>

namespace gungnir::queue {

class MemoryDriver final : public Driver {
public:
    void push(
        Envelope job
    ) override {
        std::lock_guard lock{
            mutex_
        };

        pending_.push_back(
            std::move(job)
        );
    }

    void push_later(
        Envelope job,
        std::chrono::milliseconds delay
    ) override {
        if (delay.count() <= 0) {
            push(
                std::move(job)
            );

            return;
        }

        std::lock_guard lock{
            mutex_
        };

        delayed_.push_back({
            std::chrono::steady_clock::now() +
                delay,
            std::move(job)
        });
    }

    [[nodiscard]]
    std::optional<Envelope>
    pop() override {
        std::lock_guard lock{
            mutex_
        };

        promote_due_locked();

        if (pending_.empty()) {
            return std::nullopt;
        }

        auto job =
            std::move(
                pending_.front()
            );

        pending_.pop_front();

        return job;
    }

    void acknowledge(
        const Envelope&
    ) override {}

    void release(
        Envelope job
    ) override {
        push(
            std::move(job)
        );
    }

    void release_after(
        Envelope job,
        std::chrono::milliseconds delay
    ) override {
        push_later(
            std::move(job),
            delay
        );
    }

    [[nodiscard]]
    bool renew(
        const Envelope&
    ) override {
        return true;
    }

    void fail(
        const Envelope& job
    ) override {
        std::lock_guard lock{
            mutex_
        };

        failed_.push_back(job);
    }

    [[nodiscard]]
    std::size_t pending() const {
        std::lock_guard lock{
            mutex_
        };

        return pending_.size();
    }

    [[nodiscard]]
    std::size_t delayed() const {
        std::lock_guard lock{
            mutex_
        };

        return delayed_.size();
    }

    [[nodiscard]]
    std::size_t failed() const {
        std::lock_guard lock{
            mutex_
        };

        return failed_.size();
    }

private:
    using DelayedJob =
        std::pair<
            std::chrono::steady_clock::time_point,
            Envelope
        >;

    void promote_due_locked() {
        const auto now =
            std::chrono::steady_clock::now();

        auto current =
            delayed_.begin();

        while (
            current !=
            delayed_.end()
        ) {
            if (
                current->first >
                now
            ) {
                ++current;
                continue;
            }

            pending_.push_back(
                std::move(
                    current->second
                )
            );

            current =
                delayed_.erase(
                    current
                );
        }
    }

    mutable std::mutex mutex_;
    std::deque<Envelope> pending_;
    std::vector<DelayedJob> delayed_;
    std::vector<Envelope> failed_;
};

} // namespace gungnir::queue
