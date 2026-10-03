#pragma once
#include <string_view>

namespace gungnir::language {

inline constexpr std::string_view language_name = "Gungnir";
inline constexpr std::string_view language_version = "development";
inline constexpr std::string_view compiler_contract_version = "development";
inline constexpr std::string_view diagnostic_contract_version = "development";
inline constexpr bool structured_profile_feature_frozen = false;
inline constexpr std::string_view source_extension = ".gnr";

enum class Compatibility {
    stable,
    deprecated,
    experimental
};

struct LanguageFeatures {
    bool inferred_bindings{true};
    bool immutable_bindings{true};
    bool classes{false};
    bool interfaces{false};
    bool enums{false};
    bool modules{true};
    bool imports{true};
    bool optionals{true};
    bool collections{true};
    bool async_await{true};
};

// Capability flags describe the structured Compiler / --strict frontend.
// Native C++ passthrough is available separately through Transpiler.
inline constexpr Compatibility compiler_compatibility = Compatibility::experimental;
inline constexpr LanguageFeatures implemented_features{};
inline constexpr LanguageFeatures stable_features = implemented_features; // Legacy API name; not a stability guarantee.

} // namespace gungnir::language

