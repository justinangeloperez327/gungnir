#include <gungnir/http/cookie.hpp>
#include <stdexcept>

namespace gungnir::http {

std::string serialize_cookie(const Cookie& cookie) {
    if (cookie.name.empty() || cookie.name.find_first_of("=;\r\n") != std::string::npos ||
        cookie.value.find_first_of(";\r\n") != std::string::npos) {
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
