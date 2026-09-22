#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include <gungnir/database/backend.hpp>
#include <gungnir/database/driver.hpp>
#include <gungnir/database/settings.hpp>

namespace gungnir::database {

using ConfiguredDriverFactory =
    std::function<
        std::shared_ptr<Driver>(
            const Settings&
        )
    >;

class DriverUnavailableError :
    public std::runtime_error {
public:
    explicit DriverUnavailableError(
        Backend backend
    );
};

class DriverRegistry {
public:
    DriverRegistry& add(
        Backend backend,
        ConfiguredDriverFactory factory
    );

    [[nodiscard]] bool has(
        Backend backend
    ) const noexcept;

    [[nodiscard]] DriverFactory bind(
        Settings settings
    ) const;

private:
    std::unordered_map<
        Backend,
        ConfiguredDriverFactory
    > factories_;
};

} // namespace gungnir::database
