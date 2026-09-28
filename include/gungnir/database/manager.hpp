#pragma once

#include <cstddef>
#include <memory>
#include <string_view>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/core/types.hpp>
#include <gungnir/database/backend.hpp>
#include <gungnir/database/driver.hpp>
#include <gungnir/database/pool.hpp>

namespace gungnir::database {

class Connection;
class Transaction;
class AsyncTransaction;

class Manager {
public:
    Manager();
    ~Manager();

    Manager(Manager&&) noexcept;
    Manager& operator=(Manager&&) noexcept;

    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;

    Manager& add(
        String name,
        Backend backend,
        DriverFactory factory,
        std::size_t pool_size = 1
    );

    Manager& add(
        String name,
        Backend backend,
        DriverFactory factory,
        PoolOptions options
    );

    Manager& add_replica(
        String primary,
        DriverFactory factory,
        PoolOptions options = {}
    );

    [[nodiscard]]
    bool has(
        std::string_view name
    ) const noexcept;

    [[nodiscard]]
    std::shared_ptr<Connection>
    connection(
        std::string_view name = "default",
        CancellationToken cancellation = {}
    );

    [[nodiscard]]
    std::shared_ptr<Connection>
    read_connection(
        std::string_view name = "default",
        CancellationToken cancellation = {}
    );

    [[nodiscard]]
    std::shared_ptr<Connection>
    write_connection(
        std::string_view name = "default",
        CancellationToken cancellation = {}
    );

    [[nodiscard]]
    Backend backend(
        std::string_view name = "default"
    ) const;

    [[nodiscard]]
    Transaction transaction(
        std::string_view name = "default"
    );

    [[nodiscard]]
    AsyncTransaction async_transaction(
        std::string_view name = "default"
    );

    void validate_pools();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::database
