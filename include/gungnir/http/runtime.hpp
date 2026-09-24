#pragma once
#include <chrono>
#include <cstddef>

namespace gungnir::http {

struct RuntimeOptions {
    std::size_t max_request_bytes{1024U * 1024U};
    std::size_t max_header_bytes{64U * 1024U};
    std::size_t max_connections{4096};
    std::size_t max_requests_per_connection{100};
    std::chrono::milliseconds read_timeout{30000};
    std::chrono::milliseconds write_timeout{30000};
    std::chrono::milliseconds idle_timeout{15000};
    std::chrono::milliseconds shutdown_timeout{5000};
    bool keep_alive{true};
};

enum class HttpVersion { http_1_0, http_1_1, http_2 };
enum class ConnectionDirective { keep_alive, close };

struct ConnectionPolicy {
    HttpVersion version{HttpVersion::http_1_1};
    ConnectionDirective directive{ConnectionDirective::keep_alive};
    std::size_t requests_served{0};
};

} // namespace gungnir::http
