#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/core/types.hpp>
#include <gungnir/database/driver.hpp>
#include <gungnir/database/result.hpp>
#include <gungnir/database/query.hpp>

namespace gungnir::database {

class Transaction;
class AsyncTransaction;

struct TransactionToken {
    bool root{false};
    String savepoint;
    std::size_t depth{0};
    std::uint64_t scope_id{0};
};

class Connection {
public:
    Connection(String name, std::shared_ptr<Driver> driver);

    [[nodiscard]] const String& name() const noexcept;
    [[nodiscard]] Backend backend() const noexcept;

    Result execute(
        const String& statement,
        const std::vector<model::AttributeValue>& bindings = {}
    );

    Result execute(
        const String& statement,
        const std::vector<model::AttributeValue>& bindings,
        const CancellationToken& cancellation
    );

    Result execute(const Query& query);

    Result execute(
        const Query& query,
        const CancellationToken& cancellation
    );

    [[nodiscard]] bool supports_transactions() const noexcept;
    [[nodiscard]] bool supports_savepoints() const noexcept;

    void after_commit(std::function<void()> callback);
    [[nodiscard]] bool in_transaction() const;
    void begin();
    void commit();
    void rollback();

    [[nodiscard]]
    TransactionToken begin_scope();

    void commit_scope(
        const TransactionToken& token
    );

    void rollback_scope(
        const TransactionToken& token
    );

    [[nodiscard]]
    bool healthy();

private:
    friend class Transaction;
    friend class AsyncTransaction;

    [[nodiscard]]
    String next_savepoint_name();

    void validate_scope_token(
        const TransactionToken& token
    ) const;

    String name_;
    std::shared_ptr<Driver> driver_;
    mutable std::recursive_mutex mutex_;
    std::vector<std::vector<std::function<void()>>> commit_callbacks_;
    std::size_t transaction_depth_{0};
    std::vector<std::uint64_t> transaction_scope_ids_;
    std::uint64_t transaction_scope_sequence_{0};
    std::uint64_t savepoint_sequence_{0};
};

} // namespace gungnir::database
