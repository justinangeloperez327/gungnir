#pragma once

#include <algorithm>
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
#include <vector>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/queue/driver.hpp>

namespace gungnir::queue {

struct WorkerOptions {
    std::chrono::milliseconds idle_sleep{100};
    std::chrono::milliseconds lease_renewal_interval{0};
    std::vector<std::chrono::milliseconds> retry_backoff;
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

        auto span =
            observability::
                global_tracer()
                ->start_span(
                    "queue.job",
                    {
                        {
                            "messaging.operation",
                            "process"
                        },
                        {
                            "messaging.message.id",
                            job->id
                        },
                        {
                            "messaging.destination.name",
                            job->name
                        },
                        {
                            "messaging.message.retry.count",
                            std::to_string(
                                job->attempts
                            )
                        }
                    },
                    trace_parent(*job)
                );

        auto span_scope =
            span.valid()
                ? span.scope()
                : observability::Scope{};

        const auto found =
            handlers_.find(job->name);

        if (
            found ==
            handlers_.end()
        ) {
            span.error(
                "No queue handler is registered"
            );

            span.end();
            driver_->fail(*job);
            return true;
        }

        ++job->attempts;

        span.attribute(
            "messaging.message.retry.count",
            std::to_string(
                job->attempts
            )
        );

        LeaseHeartbeat heartbeat{
            *driver_,
            *job,
            options_.lease_renewal_interval
        };

        const auto handle_failure =
            [&] {
                if (
                    job->attempts <
                    job->max_attempts
                ) {
                    const auto delay =
                        retry_delay(
                            job->attempts
                        );

                    driver_->release_after(
                        std::move(*job),
                        delay
                    );
                } else {
                    driver_->fail(*job);
                }
            };

        try {
            found->second(job->payload);
            heartbeat.stop();

            span.status(
                observability::
                    SpanStatus::ok
            );

            span.end();

            driver_->acknowledge(*job);
        } catch (
            const std::exception& error
        ) {
            heartbeat.stop();

            span.error(
                error.what()
            );

            span.end();
            handle_failure();
        } catch (...) {
            heartbeat.stop();

            span.error(
                "Unknown queue handler exception"
            );

            span.end();
            handle_failure();
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
                .lease_renewal_interval
                .count() < 0
        ) {
            throw std::invalid_argument(
                "Queue worker lease_renewal_interval cannot be negative"
            );
        }

        for (
            const auto delay :
            options_.retry_backoff
        ) {
            if (delay.count() < 0) {
                throw std::invalid_argument(
                    "Queue worker retry backoff cannot be negative"
                );
            }
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

    [[nodiscard]]
    std::chrono::milliseconds
    retry_delay(
        unsigned attempts
    ) const noexcept {
        if (
            options_
                .retry_backoff
                .empty()
        ) {
            return
                std::chrono::milliseconds{
                    0
                };
        }

        const auto index =
            std::min<std::size_t>(
                attempts == 0
                    ? 0
                    : static_cast<
                        std::size_t
                      >(attempts - 1),
                options_
                    .retry_backoff
                    .size() - 1
            );

        return
            options_
                .retry_backoff[
                    index
                ];
    }

    class LeaseHeartbeat {
    public:
        LeaseHeartbeat(
            Driver& driver,
            const Envelope& job,
            std::chrono::milliseconds interval
        )
            : driver_(&driver),
              job_(&job),
              interval_(interval) {
            if (interval_.count() <= 0) {
                return;
            }

            thread_ =
                std::thread{
                    [this] {
                        loop();
                    }
                };
        }

        LeaseHeartbeat(
            const LeaseHeartbeat&
        ) = delete;

        LeaseHeartbeat& operator=(
            const LeaseHeartbeat&
        ) = delete;

        ~LeaseHeartbeat() {
            stop();
        }

        void stop()
            noexcept {
            {
                std::lock_guard lock{
                    mutex_
                };

                stopped_ = true;
            }

            ready_.notify_all();

            if (
                thread_.joinable()
            ) {
                thread_.join();
            }
        }

        [[nodiscard]]
        bool lost()
            const noexcept {
            return
                lost_.load(
                    std::memory_order_acquire
                );
        }

    private:
        void loop()
            noexcept {
            std::unique_lock lock{
                mutex_
            };

            while (!stopped_) {
                if (
                    ready_.wait_for(
                        lock,
                        interval_,
                        [this] {
                            return stopped_;
                        }
                    )
                ) {
                    return;
                }

                lock.unlock();

                bool renewed = false;

                try {
                    renewed =
                        driver_->renew(
                            *job_
                        );
                } catch (...) {
                    renewed = false;
                }

                lock.lock();

                if (!renewed) {
                    lost_.store(
                        true,
                        std::memory_order_release
                    );

                    return;
                }
            }
        }

        Driver* driver_;
        const Envelope* job_;
        std::chrono::milliseconds interval_;
        std::atomic_bool lost_{false};
        std::mutex mutex_;
        std::condition_variable ready_;
        bool stopped_{false};
        std::thread thread_;
    };

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
