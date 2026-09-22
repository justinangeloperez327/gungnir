#include <gungnir/database/settings.hpp>

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>

namespace gungnir::database {

namespace {

String lower(std::string_view value) {
    String result{value};

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char character) {
            return static_cast<char>(
                std::tolower(character)
            );
        }
    );

    return result;
}

std::size_t checked_pool_size(
    Int64 value
) {
    if (value <= 0) {
        throw std::out_of_range(
            "database.pool_size must be greater than zero"
        );
    }

    return static_cast<std::size_t>(
        value
    );
}

std::uint16_t checked_port(
    Int64 value
) {
    if (
        value <= 0 ||
        value >
            static_cast<Int64>(
                std::numeric_limits<
                    std::uint16_t
                >::max()
            )
    ) {
        throw std::out_of_range(
            "database.port must be between 1 and 65535"
        );
    }

    return static_cast<std::uint16_t>(
        value
    );
}

} // namespace

Backend parse_backend(
    std::string_view value
) {
    const auto normalized =
        lower(value);

    if (
        normalized == "postgresql" ||
        normalized == "postgres" ||
        normalized == "pgsql"
    ) {
        return Backend::postgresql;
    }

    if (
        normalized == "mysql" ||
        normalized == "mariadb"
    ) {
        return Backend::mysql;
    }

    if (
        normalized == "mssql" ||
        normalized == "sqlserver" ||
        normalized == "sql_server"
    ) {
        return Backend::mssql;
    }

    if (
        normalized == "mongodb" ||
        normalized == "mongo"
    ) {
        return Backend::mongodb;
    }

    throw std::invalid_argument(
        "Unsupported Gungnir database backend '" +
        String{value} +
        "'"
    );
}

std::uint16_t default_port(
    Backend backend
) noexcept {
    switch (backend) {
    case Backend::postgresql:
        return 5432;
    case Backend::mysql:
        return 3306;
    case Backend::mssql:
        return 1433;
    case Backend::mongodb:
        return 27017;
    }

    return 0;
}

Settings settings_from(
    const config::Repository& config
) {
    const auto driver =
        config.string(
            "database.default"
        );

    if (driver.empty()) {
        throw std::logic_error(
            "No database backend is configured. Set DB_CONNECTION first."
        );
    }

    Settings settings;
    settings.name =
        config.string(
            "database.name",
            "default"
        );

    settings.backend =
        parse_backend(driver);

    settings.host =
        config.string(
            "database.host",
            "127.0.0.1"
        );

    settings.database =
        config.string(
            "database.database"
        );

    settings.username =
        config.string(
            "database.username"
        );

    settings.password =
        config.string(
            "database.password"
        );

    settings.pool_size =
        checked_pool_size(
            config.integer(
                "database.pool_size",
                1
            )
        );

    settings.port =
        checked_port(
            config.integer(
                "database.port",
                static_cast<Int64>(
                    default_port(
                        settings.backend
                    )
                )
            )
        );

    return settings;
}

} // namespace gungnir::database
