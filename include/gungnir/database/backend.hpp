#pragma once

#include <string_view>

namespace gungnir::database {

enum class Backend {
    postgresql,
    mysql,
    mssql,
    mongodb
};

[[nodiscard]] constexpr std::string_view name(Backend backend) noexcept {
    switch (backend) {
        case Backend::postgresql: return "postgresql";
        case Backend::mysql: return "mysql";
        case Backend::mssql: return "mssql";
        case Backend::mongodb: return "mongodb";
    }

    return "unknown";
}

} // namespace gungnir::database
