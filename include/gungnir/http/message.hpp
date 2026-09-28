#pragma once

#include <string>
#include <string_view>

#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>
#include <gungnir/http/runtime.hpp>

namespace gungnir::http::wire {

[[nodiscard]] Request parse_request(
    std::string_view message,
    CancellationToken cancellation = {},
    bool secure = false
);

[[nodiscard]] std::string serialize_response(
    const Response& response,
    bool omit_body = false,
    ConnectionDirective connection = ConnectionDirective::close
);

[[nodiscard]]
std::string serialize_stream_headers(
    const Response& response,
    bool omit_body = false,
    ConnectionDirective connection =
        ConnectionDirective::close
);

[[nodiscard]]
std::string serialize_chunk(
    std::string_view chunk
);

[[nodiscard]]
std::string serialize_chunk_end();

[[nodiscard]] bool request_keep_alive(const Request& request) noexcept;

} // namespace gungnir::http::wire
