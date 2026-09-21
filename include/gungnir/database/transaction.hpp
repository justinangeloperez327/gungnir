#pragma once

#include <memory>
#include <mutex>

#include <gungnir/database/connection.hpp>

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

    [[nodiscard]] bool active() const noexcept;

private:
    std::shared_ptr<Connection> connection_;
    std::unique_lock<std::recursive_mutex> lock_;
    bool active_{false};
};

} // namespace gungnir::database
