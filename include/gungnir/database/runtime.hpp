#pragma once

#include <memory>
#include <string_view>

#include <gungnir/database/manager.hpp>

namespace gungnir::database::runtime {

void use(Manager& manager) noexcept;
void clear() noexcept;

[[nodiscard]] bool configured() noexcept;
[[nodiscard]] bool using_manager(const Manager& manager) noexcept;
[[nodiscard]] Manager& manager();

[[nodiscard]] std::shared_ptr<Connection> connection(
    std::string_view name = "default"
);

} // namespace gungnir::database::runtime
