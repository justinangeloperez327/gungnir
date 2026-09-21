#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <unordered_map>

#include <gungnir/core/types.hpp>

namespace gungnir::config {

class Environment {
public:
    Environment() = default;

    Environment& load(
        const std::filesystem::path& path,
        bool optional = true,
        bool overwrite = true
    );

    Environment& set(String key, String value);

    [[nodiscard]] bool has(std::string_view key) const;
    [[nodiscard]] std::optional<String> find(
        std::string_view key
    ) const;

    [[nodiscard]] String get(
        std::string_view key,
        String fallback = {}
    ) const;

    [[nodiscard]] Int64 integer(
        std::string_view key,
        Int64 fallback = 0
    ) const;

    [[nodiscard]] Boolean boolean(
        std::string_view key,
        Boolean fallback = false
    ) const;

    [[nodiscard]] const std::unordered_map<String, String>& loaded()
        const noexcept;

private:
    std::unordered_map<String, String> values_;
};

} // namespace gungnir::config
