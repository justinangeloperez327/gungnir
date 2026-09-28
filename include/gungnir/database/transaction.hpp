#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
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

} // namespace detail

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
    const Connection& connection() const;

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
        ensure_owner();

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
        const;

private:
    void ensure_owner() const;

    std::shared_ptr<Connection>
        connection_;

    std::unique_lock<
        std::recursive_mutex
    > lock_;

    std::thread::id owner_;
    bool active_{false};
};

} // namespace gungnir::database
