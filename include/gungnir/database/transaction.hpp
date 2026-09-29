#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <gungnir/core/task.hpp>
#include <gungnir/database/connection.hpp>
#include <gungnir/database/runtime.hpp>

namespace gungnir::database {

namespace detail {

template <typename Value>
struct IsTask :
    std::false_type {};

template <typename Value>
struct IsTask<
    ::gungnir::Task<Value>
> :
    std::true_type {};

template <typename Value>
inline constexpr bool is_task_v =
    IsTask<
        std::remove_cvref_t<Value>
    >::value;

template <typename Value>
struct TaskValue;

template <typename Value>
struct TaskValue<
    ::gungnir::Task<Value>
> {
    using type = Value;
};

template <typename Value>
using task_value_t =
    typename TaskValue<
        std::remove_cvref_t<Value>
    >::type;

} // namespace detail

struct AsyncTransactionOptions {
    std::chrono::milliseconds
        timeout{
            std::chrono::seconds{30}
        };
};

class Transaction {
public:
    explicit Transaction(
        std::shared_ptr<Connection> connection
    );

    ~Transaction();

    Transaction(
        const Transaction&
    ) = delete;

    Transaction& operator=(
        const Transaction&
    ) = delete;

    Transaction(
        Transaction&&
    ) = delete;

    Transaction& operator=(
        Transaction&&
    ) = delete;

    [[nodiscard]]
    Connection& connection();

    [[nodiscard]]
    const Connection& connection()
        const;

    void commit();
    void rollback();

    template <typename Callback>
    requires (
        !detail::is_task_v<
            std::invoke_result_t<
                Callback&
            >
        >
    )
    auto run(
        Callback&& callback
    ) {
        if (!active_) {
            throw std::logic_error(
                "Cannot run work on an inactive database transaction"
            );
        }

        runtime::detail::
            ConnectionScope scope{
                connection_
            };

        try {
            using CallbackResult =
                std::invoke_result_t<
                    Callback&
                >;

            if constexpr (
                std::is_void_v<
                    CallbackResult
                >
            ) {
                std::invoke(
                    callback
                );

                commit();
            } else {
                auto result =
                    std::invoke(
                        callback
                    );

                commit();

                return result;
            }
        } catch (...) {
            rollback();
            throw;
        }
    }

    [[nodiscard]]
    bool active()
        const noexcept;

private:
    std::shared_ptr<Connection>
        connection_;
    TransactionToken token_;
    bool active_{false};
};

class AsyncTransaction {
public:
    explicit AsyncTransaction(
        std::shared_ptr<Connection> connection,
        AsyncTransactionOptions options = {}
    );

    ~AsyncTransaction();

    AsyncTransaction(
        const AsyncTransaction&
    ) = delete;

    AsyncTransaction& operator=(
        const AsyncTransaction&
    ) = delete;

    AsyncTransaction(
        AsyncTransaction&&
    ) = delete;

    AsyncTransaction& operator=(
        AsyncTransaction&&
    ) = delete;

    [[nodiscard]]
    Connection& connection();

    [[nodiscard]]
    const Connection& connection()
        const;

    void commit();
    void rollback();

    [[nodiscard]]
    bool active()
        const noexcept;

    [[nodiscard]]
    bool expired()
        const noexcept;

    template <typename Callback>
    requires detail::is_task_v<
        std::invoke_result_t<
            Callback&
        >
    >
    auto run(
        Callback&& callback
    ) -> Task<
        detail::task_value_t<
            std::invoke_result_t<
                Callback&
            >
        >
    > {
        using CallbackTask =
            std::invoke_result_t<
                Callback&
            >;

        using Result =
            detail::task_value_t<
                CallbackTask
            >;

        if (!active_) {
            throw std::logic_error(
                "Cannot run work on an inactive async database transaction"
            );
        }

        runtime::detail::
            ConnectionScope scope{
                connection_
            };

        try {
            if constexpr (
                std::is_void_v<Result>
            ) {
                co_await std::invoke(
                    callback
                );

                ensure_not_expired();
                commit();
                co_return;
            } else {
                auto result =
                    co_await std::invoke(
                        callback
                    );

                ensure_not_expired();
                commit();

                co_return result;
            }
        } catch (...) {
            rollback();
            throw;
        }
    }

private:
    void ensure_not_expired()
        const;

    std::shared_ptr<Connection>
        connection_;
    TransactionToken token_;
    AsyncTransactionOptions options_;
    std::chrono::steady_clock::time_point
        started_;
    bool active_{false};
};

} // namespace gungnir::database
