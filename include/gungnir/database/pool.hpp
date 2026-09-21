#pragma once

#include <cstddef>
#include <memory>

#include <gungnir/core/types.hpp>
#include <gungnir/database/backend.hpp>
#include <gungnir/database/connection.hpp>
#include <gungnir/database/driver.hpp>

namespace gungnir::database {

class ConnectionPool {
public:
    ConnectionPool(
        String name,
        Backend backend,
        DriverFactory factory,
        std::size_t size = 1
    );

    [[nodiscard]] std::shared_ptr<Connection> acquire();
    [[nodiscard]] const String& name() const noexcept;
    [[nodiscard]] Backend backend() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace gungnir::database
