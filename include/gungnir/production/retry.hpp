#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

#include <gungnir/core/cancellation.hpp>

namespace gungnir::production {

struct RetryOptions {
    std::size_t max_attempts{3};

    std::chrono::milliseconds
        initial_backoff{100};

    std::chrono::milliseconds
        max_backoff{2000};

    double multiplier{2.0};
};

using RetryPredicate =
    std::function<
        bool(
            const std::exception_ptr&,
            std::size_t
        )
    >;

inline void validate_retry_options(
    const RetryOptions& options
) {
    if (options.max_attempts == 0) {
        throw std::invalid_argument(
            "Retry max_attempts must be greater than zero"
        );
    }

    if (
        options.initial_backoff.count() < 0 ||
        options.max_backoff.count() < 0 ||
        options.max_backoff <
            options.initial_backoff
    ) {
        throw std::invalid_argument(
            "Retry backoff bounds are invalid"
        );
    }

    if (options.multiplier < 1.0) {
        throw std::invalid_argument(
            "Retry multiplier must be at least one"
        );
    }
}

inline void retry_sleep(
    std::chrono::milliseconds delay,
    const CancellationToken& cancellation
) {
    cancellation.throw_if_cancelled();

    if (delay <= std::chrono::milliseconds::zero()) {
        return;
    }

    std::mutex mutex;
    std::condition_variable ready;
    bool cancelled = false;

    auto registration =
        cancellation.on_cancel(
            [&] {
                {
                    std::lock_guard lock{
                        mutex
                    };

                    cancelled = true;
                }

                ready.notify_all();
            }
        );

    std::unique_lock lock{mutex};

    ready.wait_for(
        lock,
        delay,
        [&] {
            return cancelled;
        }
    );

    cancellation.throw_if_cancelled();
}

inline std::chrono::milliseconds
next_retry_backoff(
    std::chrono::milliseconds current,
    const RetryOptions& options
) noexcept {
    if (
        current >=
        options.max_backoff
    ) {
        return options.max_backoff;
    }

    const auto scaled =
        static_cast<long double>(
            current.count()
        ) *
        static_cast<long double>(
            options.multiplier
        );

    const auto maximum =
        static_cast<long double>(
            options.max_backoff.count()
        );

    if (scaled >= maximum) {
        return options.max_backoff;
    }

    return std::chrono::milliseconds{
        static_cast<
            std::chrono::milliseconds::rep
        >(scaled)
    };
}

template <typename Operation>
decltype(auto) retry(
    RetryOptions options,
    Operation&& operation,
    RetryPredicate should_retry,
    CancellationToken cancellation = {}
) {
    validate_retry_options(options);

    if (!should_retry) {
        throw std::invalid_argument(
            "Retry requires a retry predicate"
        );
    }

    using Result =
        std::invoke_result_t<Operation&>;

    auto delay =
        options.initial_backoff;

    for (
        std::size_t attempt = 1;
        attempt <= options.max_attempts;
        ++attempt
    ) {
        cancellation.throw_if_cancelled();

        try {
            if constexpr (
                std::is_void_v<Result>
            ) {
                std::invoke(operation);
                return;
            } else {
                return std::invoke(
                    operation
                );
            }
        } catch (
            const OperationCancelled&
        ) {
            throw;
        } catch (...) {
            const auto failure =
                std::current_exception();

            if (
                attempt >=
                    options.max_attempts ||
                !should_retry(
                    failure,
                    attempt
                )
            ) {
                std::rethrow_exception(
                    failure
                );
            }

            retry_sleep(
                delay,
                cancellation
            );

            delay =
                next_retry_backoff(
                    delay,
                    options
                );
        }
    }

    throw std::logic_error(
        "Retry exhausted without a result"
    );
}

} // namespace gungnir::production
