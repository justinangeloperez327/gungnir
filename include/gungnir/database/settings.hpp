#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <gungnir/config/repository.hpp>
#include <gungnir/core/types.hpp>
#include <gungnir/database/backend.hpp>

namespace gungnir::database {

struct Settings {
    String name{"default"};
    Backend backend{Backend::postgresql};
    String host{"127.0.0.1"};
    std::uint16_t port{5432};
    String database;
    String username;
    String password;
    std::size_t pool_size{1};
    std::chrono::milliseconds
        pool_acquire_timeout{
            std::chrono::seconds{5}
        };
    std::chrono::milliseconds
        pool_validation_interval{
            std::chrono::seconds{30}
        };
    std::size_t pool_reconnect_attempts{1};
    String options;
};

[[nodiscard]] Backend parse_backend(
    std::string_view value
);

[[nodiscard]] std::uint16_t default_port(
    Backend backend
) noexcept;

[[nodiscard]] Settings settings_from(
    const config::Repository& config
);

} // namespace gungnir::database
