#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace gungnir::validation {

struct ParsedRule {
    std::string name;
    std::vector<std::string> arguments;
};

inline std::string_view rule_trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r' || text.front() == '\n')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' || text.back() == '\n')) text.remove_suffix(1);
    return text;
}

inline bool rule_identifier(std::string_view text) {
    auto alpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    return !text.empty() && alpha(text.front()) && std::all_of(text.begin(), text.end(), [&](char c) { return alpha(c) || (c >= '0' && c <= '9'); });
}

inline bool rule_path(std::string_view text, bool wildcard = true) {
    if (text.empty()) return false;
    while (true) {
        const auto dot = text.find('.');
        const auto part = text.substr(0, dot);
        if (part.empty() || (part == "*" ? !wildcard : !std::all_of(part.begin(), part.end(), [](unsigned char c) { return c > 32 && c != 127 && c != '*' && c != '|' && c != ':' && c != ','; }))) return false;
        if (dot == std::string_view::npos) return true;
        text.remove_prefix(dot + 1);
    }
}

// Shared by the native runtime and ProgramValidator. Checks configuration even
// for absent/sometimes fields, before any input or database work takes place.
inline std::vector<ParsedRule> parse_rule_expression(std::string_view field, std::string_view expression) {
    if (!rule_path(field)) throw std::logic_error("Invalid validation field path");
    std::vector<ParsedRule> result;
    while (true) {
        const auto pipe = expression.find('|');
        const auto token = rule_trim(expression.substr(0, pipe));
        if (token.empty()) throw std::logic_error("Validation rules must not contain empty entries");
        const auto colon = token.find(':');
        ParsedRule rule{std::string{rule_trim(token.substr(0, colon))}, {}};
        if (rule.name == "not_in") rule.name = "notIn";
        if (rule.name == "required_if") rule.name = "requiredIf";
        if (rule.name == "dateTime") rule.name = "datetime";
        if (colon != std::string_view::npos) {
            auto arguments = token.substr(colon + 1);
            while (true) {
                const auto comma = arguments.find(',');
                const auto argument = rule_trim(arguments.substr(0, comma));
                if (argument.empty()) throw std::logic_error("Validation rule arguments must not be empty");
                rule.arguments.emplace_back(argument);
                if (comma == std::string_view::npos) break;
                arguments.remove_prefix(comma + 1);
            }
        }
        const auto& name = rule.name;
        const auto count = rule.arguments.size();
        const bool flag = name == "required" || name == "present" || name == "nullable" || name == "sometimes" || name == "bail" || name == "string" || name == "integer" || name == "numeric" || name == "boolean" || name == "email" || name == "accepted" || name == "array" || name == "object" || name == "uuid" || name == "url" || name == "date" || name == "datetime" || name == "file" || name == "image" || name == "confirmed";
        const bool size = name == "min" || name == "max" || name == "size" || name == "length";
        const bool list = name == "in" || name == "notIn" || name == "mimes" || name == "mimetypes" || name == "extensions";
        const bool other = name == "same" || name == "different" || name == "custom";
        const bool database = name == "unique" || name == "exists";
        if (!(flag || size || list || other || database || name == "requiredIf")) throw std::logic_error("Unknown validation rule '" + name + "'");
        if ((flag && count != 0) || ((size || other) && count != 1) || (list && count == 0) || (name == "requiredIf" && count < 2) || (database && (count < 1 || count > (name == "unique" ? 4U : 2U)))) throw std::logic_error("Invalid argument count for validation rule '" + name + "'");
        if (size) {
            double value{};
            const auto& argument = rule.arguments.front();
            const auto [end, error] = std::from_chars(argument.data(), argument.data() + argument.size(), value);
            if (error != std::errc{} || end != argument.data() + argument.size() || !std::isfinite(value)) throw std::logic_error("Validation size arguments must be finite numbers");
            if (name == "length") {
                std::uint64_t length{};
                const auto [length_end, length_error] = std::from_chars(argument.data(), argument.data() + argument.size(), length);
                if (length_error != std::errc{} || length_end != argument.data() + argument.size()) throw std::logic_error("Validation length requires a non-negative 64-bit integer");
            }
        }
        if ((name == "same" || name == "different" || name == "requiredIf") && !rule_path(rule.arguments.front())) throw std::logic_error("Invalid validation comparison path");
        if (name == "custom" && !rule_identifier(rule.arguments.front())) throw std::logic_error("Invalid custom validation rule name");
        if (database) {
            if (!rule_identifier(rule.arguments[0]) || !rule_identifier(count > 1 ? rule.arguments[1] : field) || (count == 4 && !rule_identifier(rule.arguments[3]))) throw std::logic_error("Invalid database validation identifier");
        }
        if (name == "mimes") for (const auto& format : rule.arguments) {
            if (format != "png" && format != "jpeg" && format != "jpg" && format != "gif" && format != "webp" && format != "pdf" && format != "txt" && format != "bin") throw std::logic_error("Unsupported upload format '" + format + "'");
        }
        if (name == "mimetypes") for (const auto& media : rule.arguments) {
            const auto slash = media.find('/');
            auto token = [](std::string_view part) {
                return !part.empty() && std::all_of(part.begin(), part.end(), [](char c) {
                    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || std::string_view{"!#$%&'*+-.^_`|~"}.find(c) != std::string_view::npos;
                });
            };
            if (slash == std::string::npos || !token(std::string_view{media}.substr(0, slash)) || !token(std::string_view{media}.substr(slash + 1))) throw std::logic_error("Invalid upload media type");
        }
        if (name == "extensions") for (const auto& extension : rule.arguments) if (!std::all_of(extension.begin(), extension.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); })) throw std::logic_error("Upload extensions must be lower-case letters or digits");
        result.push_back(std::move(rule));
        if (pipe == std::string_view::npos) break;
        expression.remove_prefix(pipe + 1);
    }
    return result;
}

} // namespace gungnir::validation
