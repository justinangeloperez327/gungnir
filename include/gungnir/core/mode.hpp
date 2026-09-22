#pragma once
#include <string_view>

namespace gungnir {

enum class ApplicationMode { development, testing, staging, production };

[[nodiscard]] inline ApplicationMode application_mode(std::string_view value) noexcept {
    if (value == "production" || value == "prod") return ApplicationMode::production;
    if (value == "testing" || value == "test") return ApplicationMode::testing;
    if (value == "staging" || value == "stage") return ApplicationMode::staging;
    return ApplicationMode::development;
}

[[nodiscard]] inline bool production(ApplicationMode mode) noexcept { return mode == ApplicationMode::production; }

} // namespace gungnir
