#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace gungnir::http {

struct TlsOptions {
    std::string certificate_chain;
    std::string private_key;
    std::string private_key_password;
    std::string client_ca;
    bool require_client_certificate{false};

    // ALPN is transport negotiation only. HTTP/1.1 is the
    // production protocol implemented by the core server today.
    std::vector<std::string>
        alpn_protocols{
            "http/1.1"
        };
};

struct RuntimeOptions {
    std::size_t max_request_bytes{1024U * 1024U};
    std::size_t max_header_bytes{64U * 1024U};
    std::size_t max_connections{4096};
    std::size_t max_requests_per_connection{100};
    std::size_t max_stream_chunk_bytes{64U * 1024U};
    std::chrono::milliseconds read_timeout{30000};
    std::chrono::milliseconds write_timeout{30000};
    std::chrono::milliseconds idle_timeout{15000};
    std::chrono::milliseconds request_timeout{30000};
    std::chrono::milliseconds stream_chunk_timeout{30000};
    std::chrono::milliseconds shutdown_timeout{5000};
    bool keep_alive{true};
    std::optional<TlsOptions> tls;
};

enum class HttpVersion {
    http_1_0,
    http_1_1,
    http_2
};

enum class ConnectionDirective {
    keep_alive,
    close
};

struct ConnectionPolicy {
    HttpVersion version{
        HttpVersion::http_1_1
    };
    ConnectionDirective directive{
        ConnectionDirective::keep_alive
    };
    std::size_t requests_served{0};
};

} // namespace gungnir::http
