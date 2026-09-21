#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <gungnir/database/connection.hpp>
#include <gungnir/database/runtime.hpp>

namespace gungnir::database {

class Transaction {
public:
    explicit Transaction(std::shared_ptr<Connection> connection);
    ~Transaction();

    Transaction(Transaction&& other) noexcept;
    Transaction& operator=(Transaction&& other) noexcept;

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    [[nodiscard]] Connection& connection() noexcept;
    [[nodiscard]] const Connection& connection() const noexcept;

    void commit();
    void rollback();

    template <typename Callback>
    auto run(Callback&& callback) {
        if (!active_) {
            throw std::logic_error(
                "Cannot run work on an inactive database transaction"
            );
        }

        runtime::ConnectionScope scope{connection_};

        try {
            using Result = std::invoke_result_t<Callback&>;

            if constexpr (std::is_void_v<Result>) {
                std::invoke(callback);
                commit();
            } else {
                auto result = std::invoke(callback);
                commit();
                return result;
            }
        } catch (...) {
            rollback();
            throw;
        }
    }

    [[nodiscard]] bool active() const noexcept;

private:
    std::shared_ptr<Connection> connection_;
    std::unique_lock<std::recursive_mutex> lock_;
    bool active_{false};
};

} // namespace gungnir::database
