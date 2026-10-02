#pragma once

#include <chrono>
#include <functional>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

#include <gungnir/core/task.hpp>
#include <gungnir/core/timer.hpp>
#include <gungnir/database/runtime.hpp>
#include <gungnir/database/transaction.hpp>

namespace gungnir::database {

struct TransactionRetryPolicy {
    std::size_t max_attempts{1};
    std::chrono::milliseconds
        backoff{
            std::chrono::milliseconds{0}
        };

    std::function<
        bool(
            const std::exception&
        )
    > retryable;
};

class DB {
public:
    [[nodiscard]]
    static std::shared_ptr<Connection>
    connection(
        std::string_view name = "default"
    ) {
        return
            runtime::connection(name);
    }

    [[nodiscard]]
    static std::shared_ptr<Connection>
    read(
        std::string_view name = "default"
    ) {
        return
            runtime::read_connection(name);
    }

    [[nodiscard]]
    static std::shared_ptr<Connection>
    write(
        std::string_view name = "default"
    ) {
        return
            runtime::write_connection(name);
    }

    template <typename Callback>
    requires (
        !detail::is_task_v<
            std::invoke_result_t<
                Callback&
            >
        >
    )
    static auto transaction(
        Callback&& callback,
        std::string_view name =
            "default"
    ) {
        Transaction transaction{
            runtime::write_connection(
                name
            )
        };

        return transaction.run(
            std::forward<Callback>(
                callback
            )
        );
    }

    template <typename Callback>
    requires detail::is_task_v<
        std::invoke_result_t<
            Callback&
        >
    >
    static auto transaction(
        Callback&& callback,
        std::string_view name =
            "default",
        AsyncTransactionOptions options = {},
        TransactionRetryPolicy retry = {}
    ) -> Task<
        detail::task_value_t<
            std::invoke_result_t<
                Callback&
            >
        >
    > {
        using Result =
            detail::task_value_t<
                std::invoke_result_t<
                    Callback&
                >
            >;

        if (retry.max_attempts == 0) {
            throw std::invalid_argument(
                "Transaction retry attempts must be greater than zero"
            );
        }

        for (
            std::size_t attempt = 1;
            attempt <=
                retry.max_attempts;
            ++attempt
        ) {
            try {
                AsyncTransaction transaction{
                    runtime::write_connection(
                        name
                    ),
                    options
                };

                if constexpr (
                    std::is_void_v<
                        Result
                    >
                ) {
                    co_await transaction.run(
                        callback
                    );

                    co_return;
                } else {
                    co_return
                        co_await transaction.run(
                            callback
                        );
                }
            } catch (
                const std::exception& error
            ) {
                const auto can_retry =
                    attempt <
                        retry.max_attempts &&
                    retry.retryable &&
                    retry.retryable(error);

                if (!can_retry) {
                    throw;
                }
            }

            // C++ forbids await-expressions inside exception handlers.
            // Reaching this point means the caught failure was approved
            // for retry, so perform asynchronous backoff after the handler.
            if (
                retry.backoff.count() >
                0
            ) {
                co_await sleep_for(
                    retry.backoff *
                    static_cast<int>(
                        attempt
                    )
                );
            }
        }

        throw std::logic_error(
            "Database transaction retry loop exhausted unexpectedly"
        );
    }
};

} // namespace gungnir::database
