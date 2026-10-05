#pragma once

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <gungnir/http/json.hpp>

namespace gungnir::observability {
inline bool sensitive_attribute(std::string key) {
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return key.find("password") != key.npos || key.find("secret") != key.npos ||
        key.find("token") != key.npos || key.find("authorization") != key.npos ||
        key.find("cookie") != key.npos || key.find("api_key") != key.npos || key.find("apikey") != key.npos;
}
inline std::string attribute_value(const std::string& key, std::string value) {
    return sensitive_attribute(key) ? "[redacted]" : std::move(value);
}
inline std::unordered_map<std::string, std::string> attributes(const http::Json& data) {
    if (!data.is_object()) throw std::invalid_argument("Observability attributes must be an object");
    std::unordered_map<std::string, std::string> result;
    for (const auto& [key, value] : data.as_object()) {
        if (value.is_array() || value.is_object()) throw std::invalid_argument("Observability attributes require scalar values");
        result.emplace(key, attribute_value(key, value.is_string() ? value.string() : value.dump()));
    }
    return result;
}
}
