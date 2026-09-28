#pragma once

#include <memory>
#include <string_view>

#include <gungnir/database/connection.hpp>
#include <gungnir/database/manager.hpp>

namespace gungnir::database::runtime {

namespace detail {

class ConnectionScope {
public:
    explicit ConnectionScope(
        std::shared_ptr<Connection> connection
    );

    ~ConnectionScope();

    ConnectionScope(
        const ConnectionScope&
    ) = delete;

    ConnectionScope& operator=(
        const ConnectionScope&
    ) = delete;

    ConnectionScope(
        ConnectionScope&&
    ) = delete;

    ConnectionScope& operator=(
        ConnectionScope&&
    ) = delete;

private:
    std::shared_ptr<Connection> previous_;
};

} // namespace detail

void use(Manager& manager) noexcept;
void clear() noexcept;

[[nodiscard]]
bool configured() noexcept;

[[nodiscard]]
bool using_manager(
    const Manager& manager
) noexcept;

[[nodiscard]]
Manager& manager();

[[nodiscard]]
std::shared_ptr<Connection> connection(
    std::string_view name = "default"
);

} // namespace gungnir::database::runtime
