#pragma once

#include <cstddef>
#include <memory>
#include <string_view>

#include <gungnir/core/types.hpp>
#include <gungnir/database/backend.hpp>
#include <gungnir/database/driver.hpp>

namespace gungnir::database {

class Connection;
class ConnectionPool;
class Transaction;

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

    [[nodiscard]] bool has(std::string_view name) const noexcept;

    [[nodiscard]] std::shared_ptr<Connection> connection(
        std::string_view name = "default"
    );

    [[nodiscard]] Backend backend(
        std::string_view name = "default"
    ) const;

    [[nodiscard]] Transaction transaction(
        std::string_view name = "default"
    );

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::database
