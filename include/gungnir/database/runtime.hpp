#pragma once

#include <memory>
#include <gungnir/core/execution_context.hpp>
#include <string_view>
#include <utility>

#include <gungnir/database/connection.hpp>
#include <gungnir/database/manager.hpp>

namespace gungnir::database::runtime {

using ConnectionHandle =
    std::shared_ptr<Connection>;

namespace detail {

class ConnectionScope {
public:
    ConnectionScope() = default;

    explicit ConnectionScope(
        ConnectionHandle connection
    ) noexcept;

    ~ConnectionScope();

    ConnectionScope(
        const ConnectionScope&
    ) = delete;

    ConnectionScope& operator=(
        const ConnectionScope&
    ) = delete;

    ConnectionScope(
        ConnectionScope&& other
    ) noexcept;

    ConnectionScope& operator=(
        ConnectionScope&& other
    ) noexcept;

    void reset()
        noexcept;

private:
    gungnir::detail::ContextHandle owner_;
    ConnectionHandle previous_;
    bool active_{false};
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
ConnectionHandle current()
    noexcept;

[[nodiscard]]
detail::ConnectionScope activate(
    ConnectionHandle connection
) noexcept;

void clear_current()
    noexcept;

[[nodiscard]]
ConnectionHandle connection(
    std::string_view name = "default"
);

[[nodiscard]]
ConnectionHandle read_connection(
    std::string_view name = "default"
);

[[nodiscard]]
ConnectionHandle write_connection(
    std::string_view name = "default"
);

} // namespace gungnir::database::runtime

