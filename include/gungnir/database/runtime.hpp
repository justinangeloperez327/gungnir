#pragma once

#include <memory>
#include <string_view>

#include <gungnir/database/connection.hpp>
#include <gungnir/database/manager.hpp>

namespace gungnir::database::runtime {

class ConnectionScope {
public:
    explicit ConnectionScope(std::shared_ptr<Connection> connection);
    ~ConnectionScope();

    ConnectionScope(ConnectionScope&& other) noexcept;
    ConnectionScope& operator=(ConnectionScope&& other) noexcept;

    ConnectionScope(const ConnectionScope&) = delete;
    ConnectionScope& operator=(const ConnectionScope&) = delete;

private:
    std::shared_ptr<Connection> previous_;
    bool active_{true};
};

void use(Manager& manager) noexcept;
void clear() noexcept;

[[nodiscard]] bool configured() noexcept;
[[nodiscard]] bool using_manager(const Manager& manager) noexcept;
[[nodiscard]] Manager& manager();

[[nodiscard]] std::shared_ptr<Connection> connection(
    std::string_view name = "default"
);

} // namespace gungnir::database::runtime
