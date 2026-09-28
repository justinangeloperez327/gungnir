#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/queue/driver.hpp>

namespace gungnir::queue {

struct WorkerOptions {
    std::chrono::milliseconds idle_sleep{100};
    std::size_t max_jobs{0};
    std::chrono::milliseconds max_runtime{0};
};

class Worker {
public:
    using Handler = std::function<void(std::string_view)>;

    explicit Worker(
        Driver& driver,
        WorkerOptions options = {}
    )
        : driver_(&driver),
          options_(std::move(options)) {
        validate_options();
    }

    Worker& handle(
        std::string name,
        Handler handler
    ) {
        handlers_.insert_or_assign(
            std::move(name),
            std::move(handler)
        );

        return *this;
    }

    [[nodiscard]]
    bool run_one() {
        auto job = driver_->pop();

        if (!job) {
            return false;
        }

        const auto found =
            handlers_.find(job->name);

        if (
            found ==
            handlers_.end()
        ) {
            driver_->fail(*job);
            return true;
        }

        ++job->attempts;

        try {
            found->second(job->payload);
            driver_->acknowledge(*job);
        } catch (...) {
            if (
                job->attempts <
                job->max_attempts
            ) {
                driver_->release(
                    std::move(*job)
                );
            } else {
                driver_->fail(*job);
            }
        }

        return true;
    }

    [[nodiscard]]
    std::size_t run(
        CancellationToken cancellation = {}
    ) {
        bool expected = false;

        if (
            !running_
                .compare_exchange_strong(
                    expected,
                    true,
                    std::memory_order_acq_rel
                )
        ) {
            throw std::logic_error(
                "Queue worker is already running"
            );
        }

        const auto started =
            std::chrono::steady_clock::now();

        std::size_t processed = 0;

        const auto registration =
            cancellation.on_cancel(
                [this] {
                    wake();
                }
            );

        try {
            while (
                !stop_requested() &&
                !cancellation.cancelled()
            ) {
                if (
                    options_.max_jobs != 0 &&
                    processed >=
                        options_.max_jobs
                ) {
                    break;
                }

                if (
                    options_
                        .max_runtime
                        .count() > 0 &&
                    std::chrono::steady_clock::now() -
                        started >=
                    options_.max_runtime
                ) {
                    break;
                }

                if (run_one()) {
                    ++processed;
                    continue;
                }

                std::unique_lock lock{
                    stop_mutex_
                };

                stop_ready_.wait_for(
                    lock,
                    options_.idle_sleep,
                    [this, &cancellation] {
                        return
                            stop_requested() ||
                            cancellation.cancelled();
                    }
                );
            }
        } catch (...) {
            running_.store(
                false,
                std::memory_order_release
            );

            throw;
        }

        running_.store(
            false,
            std::memory_order_release
        );

        return processed;
    }

    void request_stop()
        noexcept {
        stop_requested_.store(
            true,
            std::memory_order_release
        );

        wake();
    }

    void reset_stop()
        noexcept {
        stop_requested_.store(
            false,
            std::memory_order_release
        );
    }

    [[nodiscard]]
    bool stop_requested()
        const noexcept {
        return
            stop_requested_.load(
                std::memory_order_acquire
            );
    }

    [[nodiscard]]
    bool running()
        const noexcept {
        return
            running_.load(
                std::memory_order_acquire
            );
    }

    [[nodiscard]]
    const WorkerOptions& options()
        const noexcept {
        return options_;
    }

private:
    void validate_options()
        const {
        if (
            options_
                .idle_sleep
                .count() <= 0
        ) {
            throw std::invalid_argument(
                "Queue worker idle_sleep must be greater than zero"
            );
        }

        if (
            options_
                .max_runtime
                .count() < 0
        ) {
            throw std::invalid_argument(
                "Queue worker max_runtime cannot be negative"
            );
        }
    }

    void wake()
        noexcept {
        stop_ready_.notify_all();
    }

    Driver* driver_;
    WorkerOptions options_;
    std::unordered_map<
        std::string,
        Handler
    > handlers_;
    std::atomic_bool stop_requested_{
        false
    };
    std::atomic_bool running_{
        false
    };
    std::mutex stop_mutex_;
    std::condition_variable stop_ready_;
};

} // namespace gungnir::queue
