#pragma once

#include <string>
#include <string_view>

#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>

namespace gungnir::http::wire {

[[nodiscard]] Request parse_request(std::string_view message);

[[nodiscard]] std::string serialize_response(
    const Response& response,
    bool omit_body = false
);

} // namespace gungnir::http::wire
