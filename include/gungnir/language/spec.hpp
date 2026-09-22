#pragma once
#include <string_view>

namespace gungnir::language {

inline constexpr std::string_view language_name = "Gungnir";
inline constexpr std::string_view language_version = "1.0";
inline constexpr std::string_view source_extension = ".gnr";

enum class Compatibility {
    stable,
    deprecated,
    experimental
};

struct LanguageFeatures {
    bool inferred_bindings{true};
    bool immutable_bindings{true};
    bool classes{true};
    bool interfaces{true};
    bool enums{true};
    bool modules{true};
    bool imports{true};
    bool optionals{true};
    bool collections{true};
    bool async_await{true};
};

inline constexpr LanguageFeatures stable_features{};

} // namespace gungnir::language
