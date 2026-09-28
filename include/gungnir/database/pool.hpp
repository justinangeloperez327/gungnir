#pragma once

#include <chrono>
#include <cstddef>
#include <memory>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/core/types.hpp>
#include <gungnir/database/backend.hpp>
#include <gungnir/database/connection.hpp>
#include <gungnir/database/driver.hpp>

namespace gungnir::database {

struct PoolOptions {
    std::size_t size{4};
    std::chrono::milliseconds
        acquire_timeout{
            std::chrono::seconds{5}
        };
    std::chrono::milliseconds
        validation_interval{
            std::chrono::seconds{30}
        };
    std::size_t reconnect_attempts{1};
};

struct PoolStats {
    std::size_t size{0};
    std::size_t leased{0};
    std::size_t available{0};
    std::size_t reconnects{0};
    std::size_t acquire_timeouts{0};
};

class ConnectionPool {
public:
    ConnectionPool(
        String name,
        Backend backend,
        DriverFactory factory,
        std::size_t size = 1
    );

    ConnectionPool(
        String name,
        Backend backend,
        DriverFactory factory,
        PoolOptions options
    );

    [[nodiscard]]
    std::shared_ptr<Connection>
    acquire(
        CancellationToken cancellation = {}
    );

    [[nodiscard]]
    const String& name() const noexcept;

    [[nodiscard]]
    Backend backend() const noexcept;

    [[nodiscard]]
    std::size_t size() const noexcept;

    [[nodiscard]]
    PoolStats stats() const noexcept;

    void validate_idle();

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace gungnir::database
