#ifdef GUNGNIR_WITH_SQLITE
#include <gungnir/database/sqlite.hpp>
#endif
#include <gungnir/database/registry.hpp>

#include <stdexcept>
#include <utility>

namespace gungnir::database {
namespace {
String adapter_guidance(Backend backend) {
    switch (backend) {
        case Backend::sqlite:
            return "Install the Gungnir application SDK, or rebuild/install Gungnir with GUNGNIR_WITH_SQLITE=ON. Select that SDK with GUNGNIR_CMAKE_PREFIX and rebuild the application.";
        case Backend::postgresql: return "Build/install with GUNGNIR_WITH_POSTGRESQL=ON and link/register gungnir::postgresql.";
        case Backend::mysql: return "Build/install with GUNGNIR_WITH_MYSQL=ON and link/register gungnir::mysql.";
        case Backend::mssql: return "Build/install with GUNGNIR_WITH_SQLSERVER=ON and link/register gungnir::sqlserver.";
        case Backend::mongodb: return "Build/install with GUNGNIR_WITH_MONGODB=ON and register the MongoDB adapter.";
    }
    return "Install or register that backend adapter before opening a connection.";
}
}
DriverRegistry::DriverRegistry() {
#ifdef GUNGNIR_WITH_SQLITE
    add(Backend::sqlite, [](const Settings& settings) { return std::make_shared<SQLiteDriver>(settings); });
#endif
}


DriverUnavailableError::DriverUnavailableError(
    Backend backend
)
    : std::runtime_error(
        "No concrete Gungnir database driver is registered for backend '" +
        String{name(backend)} +
        "'. " + adapter_guidance(backend)
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
