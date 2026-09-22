#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include <gungnir/http/security.hpp>

namespace gungnir::security {

[[nodiscard]] bool constant_time_equal(
    std::string_view left,
    std::string_view right
) noexcept;

[[nodiscard]] bool valid_header_value(std::string_view value) noexcept;

[[nodiscard]] bool valid_cookie_name(std::string_view value) noexcept;

using CorsOptions = http::CorsOptions;
using RateLimitOptions = http::RateLimitOptions;

} // namespace gungnir::security
