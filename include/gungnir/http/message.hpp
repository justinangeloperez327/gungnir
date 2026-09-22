#pragma once

#include <string>
#include <string_view>

#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>
#include <gungnir/http/runtime.hpp>

namespace gungnir::http::wire {

[[nodiscard]] Request parse_request(std::string_view message);

[[nodiscard]] std::string serialize_response(
    const Response& response,
    bool omit_body = false,
    ConnectionDirective connection = ConnectionDirective::close
);

[[nodiscard]] bool request_keep_alive(const Request& request) noexcept;

} // namespace gungnir::http::wire
