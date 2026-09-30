#include <gungnir/http/cookie.hpp>
#include <stdexcept>
#include <algorithm>
#include <string_view>

namespace gungnir::http {

std::string serialize_cookie(const Cookie& cookie) {
    const auto token = [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') ||
               std::string_view{"!#$%&'*+-.^_`|~"}.find(c) != std::string_view::npos;
    };
    const auto attribute = [](std::string_view value) {
        return std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return c >= 0x20 && c < 0x7f && c != ';';
        });
    };
    const auto domain = [](std::string_view value) {
        if (!value.empty() && value.front() == '.') value.remove_prefix(1);
        if (value.empty() || value.size() > 253) return false;
        while (!value.empty()) {
            const auto dot = value.find('.');
            const auto label = value.substr(0, dot);
            if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-')
                return false;
            if (!std::all_of(label.begin(), label.end(), [](unsigned char c) {
                    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || c == '-';
                })) return false;
            if (dot == std::string_view::npos) return true;
            value.remove_prefix(dot + 1);
            if (value.empty()) return false;
        }
        return true;
    };
    const auto value = [](std::string_view value) {
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value.remove_prefix(1);
            value.remove_suffix(1);
        }
        return std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return c == 0x21 || (c >= 0x23 && c <= 0x2b) ||
                   (c >= 0x2d && c <= 0x3a) || (c >= 0x3c && c <= 0x5b) ||
                   (c >= 0x5d && c <= 0x7e);
        });
    };
    if (cookie.name.empty() ||
        !std::all_of(cookie.name.begin(), cookie.name.end(), token) ||
        !value(cookie.value) || !attribute(cookie.path) ||
        (cookie.domain && !domain(*cookie.domain)) ||
        (cookie.same_site == SameSite::none && !cookie.secure) ||
        (cookie.same_site != SameSite::none && cookie.same_site != SameSite::lax &&
         cookie.same_site != SameSite::strict)) {
        throw std::invalid_argument("Invalid HTTP cookie");
    }
    std::string out = cookie.name + "=" + cookie.value;
    if (!cookie.path.empty()) out += "; Path=" + cookie.path;
    if (cookie.domain) out += "; Domain=" + *cookie.domain;
    if (cookie.max_age) out += "; Max-Age=" + std::to_string(cookie.max_age->count());
    if (cookie.secure) out += "; Secure";
    if (cookie.http_only) out += "; HttpOnly";
    out += "; SameSite=";
    out += cookie.same_site == SameSite::strict ? "Strict" : cookie.same_site == SameSite::none ? "None" : "Lax";
    return out;
}

} // namespace gungnir::http

