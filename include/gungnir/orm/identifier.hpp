#pragma once

#include <cstddef>
#include <stdexcept>
#include <string_view>

#include <gungnir/core/types.hpp>
#include <gungnir/database/backend.hpp>

namespace gungnir::orm::detail {

inline void validate_identifier(std::string_view value) {
    if (value.empty()) throw std::invalid_argument("Database identifier cannot be empty");
    for (const auto c : value) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 32 || byte == 127)
            throw std::invalid_argument("Database identifier contains a control character");
    }
}

[[nodiscard]] inline String quote_identifier(database::Backend backend, std::string_view value) {
    validate_identifier(value);
    const char open = backend == database::Backend::mysql ? '`' : backend == database::Backend::mssql ? '[' : '"';
    const char close = backend == database::Backend::mssql ? ']' : open;
    String result(1, open);
    for (const auto c : value) {
        result += c;
        if (c == close) result += close;
    }
    return result + close;
}

[[nodiscard]] inline String quote_path(database::Backend backend, std::string_view value) {
    String result;
    std::size_t start = 0;
    do {
        const auto dot = value.find('.', start);
        const auto segment = value.substr(start, dot == std::string_view::npos ? dot : dot - start);
        if (!result.empty()) result += '.';
        result += segment == "*" ? "*" : quote_identifier(backend, segment);
        if (dot == std::string_view::npos) break;
        start = dot + 1;
    } while (true);
    return result;
}

// Contents of a JSON string, without its surrounding quotation marks.
[[nodiscard]] inline String json_identifier(std::string_view value) {
    validate_identifier(value);
    String result;
    for (const auto c : value) {
        if (c == '"' || c == '\\') result += '\\';
        result += c;
    }
    return result;
}

[[nodiscard]] inline String json_field_identifier(std::string_view value) {
    validate_identifier(value);
    std::size_t start = 0;
    do {
        const auto dot = value.find('.', start);
        const auto segment = value.substr(start, dot == std::string_view::npos ? dot : dot - start);
        if (segment.empty() || segment.front() == '$')
            throw std::invalid_argument("MongoDB field paths cannot contain operators or empty segments");
        if (dot == std::string_view::npos) break;
        start = dot + 1;
    } while (true);
    return json_identifier(value);
}

} // namespace gungnir::orm::detail
