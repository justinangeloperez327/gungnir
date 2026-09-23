#pragma once

#include <memory>

#include <gungnir/database/driver.hpp>
#include <gungnir/database/registry.hpp>
#include <gungnir/database/settings.hpp>

namespace gungnir::database {

[[nodiscard]] std::shared_ptr<Driver> make_sqlserver_driver(
    const Settings& settings
);

DriverRegistry& register_sqlserver(
    DriverRegistry& registry
);

} // namespace gungnir::database
