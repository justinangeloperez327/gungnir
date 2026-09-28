#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gungnir/core/cancellation.hpp>

namespace gungnir::production {

struct SupervisorOptions {
    std::chrono::milliseconds
        shutdown_timeout{
            std::chrono::seconds{30}
        };

    std::chrono::milliseconds
        poll_interval{
            std::chrono::milliseconds{10}
        };
};

struct ShutdownResult {
    bool graceful{true};
    std::chrono::milliseconds elapsed{0};
    std::vector<std::string>
        pending;
};

class Supervisor {
public:
    using StopAction =
        std::function<void()>;

    using RunningCheck =
        std::function<bool()>;

    explicit Supervisor(
        SupervisorOptions options = {}
    )
        : options_(
            std::move(options)
          ) {
        validate_options();
    }

    Supervisor(
        const Supervisor&
    ) = delete;

    Supervisor& operator=(
        const Supervisor&
    ) = delete;

    Supervisor& runtime(
        std::string name,
        StopAction request_stop,
        RunningCheck running
    ) {
        if (name.empty()) {
            throw std::invalid_argument(
                "Supervised runtime name must not be empty"
            );
        }

        if (
            !request_stop ||
            !running
        ) {
            throw std::invalid_argument(
                "Supervised runtime requires stop and running callbacks"
            );
        }

        std::lock_guard lock{
            mutex_
        };

        if (stopping_) {
            throw std::logic_error(
                "Cannot register a runtime after supervisor shutdown begins"
            );
        }

        if (
            !names_
                .insert(name)
                .second
        ) {
            throw std::logic_error(
                "Supervised runtime is already registered: " +
                name
            );
        }

        runtimes_.push_back({
            std::move(name),
            std::move(request_stop),
            std::move(running)
        });

        return *this;
    }

    [[nodiscard]]
    CancellationToken token()
        const noexcept {
        return cancellation_.token();
    }

    [[nodiscard]]
    bool stopping()
        const noexcept {
        return
            stopping_.load(
                std::memory_order_acquire
            );
    }

    [[nodiscard]]
    std::size_t size()
        const noexcept {
        std::lock_guard lock{
            mutex_
        };

        return runtimes_.size();
    }

    void request_stop()
        noexcept {
        bool expected = false;

        if (
            !stopping_
                .compare_exchange_strong(
                    expected,
                    true,
                    std::memory_order_acq_rel
                )
        ) {
            return;
        }

        cancellation_.cancel();

        std::vector<StopAction>
            stop_actions;

        {
            std::lock_guard lock{
                mutex_
            };

            stop_actions.reserve(
                runtimes_.size()
            );

            for (
                const auto& runtime :
                runtimes_
            ) {
                stop_actions.push_back(
                    runtime.request_stop
                );
            }
        }

        for (
            auto& stop :
            stop_actions
        ) {
            try {
                stop();
            } catch (...) {
                // Shutdown requests are best-effort signals.
                // Drain state is decided by the running checks.
            }
        }

        ready_.notify_all();
    }

    [[nodiscard]]
    ShutdownResult wait() {
        const auto started =
            std::chrono::steady_clock::now();

        const auto deadline =
            started +
            options_.shutdown_timeout;

        while (true) {
            auto pending =
                running_names();

            if (pending.empty()) {
                return {
                    true,
                    elapsed_since(started),
                    {}
                };
            }

            const auto now =
                std::chrono::steady_clock::now();

            if (now >= deadline) {
                return {
                    false,
                    elapsed_since(started),
                    std::move(pending)
                };
            }

            const auto remaining =
                std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(
                    deadline - now
                );

            const auto delay =
                std::min(
                    options_.poll_interval,
                    std::max(
                        std::chrono::milliseconds{1},
                        remaining
                    )
                );

            std::unique_lock lock{
                wait_mutex_
            };

            ready_.wait_for(
                lock,
                delay
            );
        }
    }

    [[nodiscard]]
    ShutdownResult shutdown() {
        request_stop();
        return wait();
    }

    void notify()
        noexcept {
        ready_.notify_all();
    }

    [[nodiscard]]
    const SupervisorOptions& options()
        const noexcept {
        return options_;
    }

private:
    struct Runtime {
        std::string name;
        StopAction request_stop;
        RunningCheck running;
    };

    void validate_options()
        const {
        if (
            options_
                .shutdown_timeout
                .count() <= 0
        ) {
            throw std::invalid_argument(
                "Supervisor shutdown timeout must be greater than zero"
            );
        }

        if (
            options_
                .poll_interval
                .count() <= 0
        ) {
            throw std::invalid_argument(
                "Supervisor poll interval must be greater than zero"
            );
        }
    }

    [[nodiscard]]
    std::vector<std::string>
    running_names()
        const {
        std::vector<Runtime>
            snapshot;

        {
            std::lock_guard lock{
                mutex_
            };

            snapshot = runtimes_;
        }

        std::vector<std::string>
            pending;

        for (
            const auto& runtime :
            snapshot
        ) {
            bool running = true;

            try {
                running =
                    runtime.running();
            } catch (...) {
                // A failed liveness check is conservatively
                // treated as still running during shutdown.
                running = true;
            }

            if (running) {
                pending.push_back(
                    runtime.name
                );
            }
        }

        return pending;
    }

    [[nodiscard]]
    static std::chrono::milliseconds
    elapsed_since(
        std::chrono::steady_clock::time_point
            started
    ) noexcept {
        return
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(
                std::chrono::steady_clock::now() -
                started
            );
    }

    SupervisorOptions options_;
    CancellationSource cancellation_;

    mutable std::mutex mutex_;
    std::unordered_set<
        std::string
    > names_;
    std::vector<Runtime> runtimes_;

    std::atomic_bool stopping_{
        false
    };

    std::mutex wait_mutex_;
    std::condition_variable ready_;
};

} // namespace gungnir::production
