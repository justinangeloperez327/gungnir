#include <gungnir/database/registry.hpp>

#include <stdexcept>
#include <utility>

namespace gungnir::database {

DriverUnavailableError::DriverUnavailableError(
    Backend backend
)
    : std::runtime_error(
        "No concrete Gungnir database driver is registered for backend '" +
        String{name(backend)} +
        "'. Install or register that backend adapter before opening a connection."
    ) {}

DriverRegistry& DriverRegistry::add(
    Backend backend,
    ConfiguredDriverFactory factory
) {
    if (!factory) {
        throw std::invalid_argument(
            "Database driver factory cannot be empty"
        );
    }

    factories_.insert_or_assign(
        backend,
        std::move(factory)
    );

    return *this;
}

bool DriverRegistry::has(
    Backend backend
) const noexcept {
    return factories_.contains(
        backend
    );
}

DriverFactory DriverRegistry::bind(
    Settings settings
) const {
    const auto found =
        factories_.find(
            settings.backend
        );

    if (
        found ==
        factories_.end()
    ) {
        throw DriverUnavailableError{
            settings.backend
        };
    }

    auto factory =
        found->second;

    return [
        factory = std::move(factory),
        settings = std::move(settings)
    ]() mutable {
        auto driver =
            factory(settings);

        if (!driver) {
            throw std::runtime_error(
                "Database driver factory returned no driver"
            );
        }

        if (
            driver->backend() !=
            settings.backend
        ) {
            throw std::logic_error(
                "Database driver factory returned the wrong backend"
            );
        }

        return driver;
    };
}

} // namespace gungnir::database
