#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace gungnir::language::detail {

// Shared framework convention for model names, relationship keys, and
// generated default table names. Keep both compiler paths in sync.
inline std::string snake_case(std::string_view value) {
    std::string result;
    for (std::size_t index = 0; index < value.size(); ++index) {
        const char current = value[index];
        if (
            std::isupper(static_cast<unsigned char>(current)) != 0 &&
            !result.empty() &&
            result.back() != '_'
        ) {
            const bool previous_lower =
                std::islower(static_cast<unsigned char>(value[index - 1])) != 0 ||
                std::isdigit(static_cast<unsigned char>(value[index - 1])) != 0;
            const bool next_lower =
                index + 1 < value.size() &&
                std::islower(static_cast<unsigned char>(value[index + 1])) != 0;
            if (previous_lower || next_lower) result.push_back('_');
        }
        if (current == '-') result.push_back('_');
        else result.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(current))));
    }
    return result;
}

inline std::string pluralize(std::string value) {
    if (value.empty()) return value;
    const auto ends_with = [&](std::string_view suffix) {
        return value.size() >= suffix.size() &&
            value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    if (
        value.size() > 1 && value.back() == 'y' &&
        std::string_view{"aeiou"}.find(value[value.size() - 2]) ==
            std::string_view::npos
    ) {
        value.pop_back();
        return value + "ies";
    }
    if (
        ends_with("s") || ends_with("x") || ends_with("z") ||
        ends_with("ch") || ends_with("sh")
    ) return value + "es";
    return value + "s";
}

inline std::string model_table_name(std::string_view name) {
    return pluralize(snake_case(name));
}

} // namespace gungnir::language::detail
