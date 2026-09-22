#pragma once
#include <chrono>
#include <optional>
#include <string>

namespace gungnir::http {

enum class SameSite { lax, strict, none };

struct Cookie {
    std::string name;
    std::string value;
    std::string path{"/"};
    std::optional<std::string> domain;
    std::optional<std::chrono::seconds> max_age;
    bool secure{false};
    bool http_only{true};
    SameSite same_site{SameSite::lax};
};

[[nodiscard]] std::string serialize_cookie(const Cookie& cookie);

} // namespace gungnir::http
