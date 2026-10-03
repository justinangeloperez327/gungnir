#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <gungnir/gungnir.hpp>
#include <gungnir/http/message.hpp>
#include <gungnir/security/security.hpp>

namespace {

using namespace std::chrono_literals;

template <typename Function>
bool throws_invalid_argument(
    Function&& function
) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

gungnir::Response run(
    gungnir::http::MiddlewareHandler middleware,
    gungnir::Request& request,
    gungnir::http::Next next =
        [](
            gungnir::Request&
        ) -> gungnir::Task<
            gungnir::Response
        > {
            co_return
                gungnir::Response::text(
                    "ok"
                );
        }
) {
    auto task =
        middleware(
            request,
            std::move(next)
        );

    task.run_inline();
    assert(task.done());

    auto awaiter =
        task.operator co_await();

    return awaiter.await_resume();
}

void header_boundaries() {
    using namespace gungnir;

    assert(
        security::valid_header_name(
            "x-request-id"
        )
    );
    assert(
        !security::valid_header_name(
            "bad header"
        )
    );
    assert(
        !security::valid_header_name(
            "bad:header"
        )
    );
    assert(
        !security::valid_header_value(
            "safe\r\ninjected: true"
        )
    );
    assert(
        !security::valid_header_value(
            std::string{"bad\0value", 9}
        )
    );

    assert(
        throws_invalid_argument(
            [] {
                Response response;
                response.header(
                    "x-test",
                    "safe\r\ninjected: true"
                );
            }
        )
    );

    assert(
        throws_invalid_argument(
            [] {
                Request request{
                    http::Method::get,
                    "/"
                };

                request.set_header(
                    "bad header",
                    "value"
                );
            }
        )
    );
}

void cookie_prefixes() {
    using namespace gungnir::http;

    assert(
        throws_invalid_argument(
            [] {
                static_cast<void>(
                    serialize_cookie(
                        Cookie{
                            .name =
                                "__Secure-token",
                            .value = "value",
                            .secure = false
                        }
                    )
                );
            }
        )
    );

    const auto secure =
        serialize_cookie(
            Cookie{
                .name =
                    "__Secure-token",
                .value = "value",
                .secure = true
            }
        );

    assert(
        secure.find("; Secure") !=
        std::string::npos
    );

    assert(
        throws_invalid_argument(
            [] {
                static_cast<void>(
                    serialize_cookie(
                        Cookie{
                            .name =
                                "__Host-token",
                            .value = "value",
                            .path = "/app",
                            .secure = true
                        }
                    )
                );
            }
        )
    );

    assert(
        throws_invalid_argument(
            [] {
                static_cast<void>(
                    serialize_cookie(
                        Cookie{
                            .name =
                                "__Host-token",
                            .value = "value",
                            .path = "/",
                            .domain =
                                "example.com",
                            .secure = true
                        }
                    )
                );
            }
        )
    );

    const auto host_only =
        serialize_cookie(
            Cookie{
                .name =
                    "__Host-token",
                .value = "value",
                .path = "/",
                .secure = true
            }
        );

    assert(
        host_only.find("Domain=") ==
        std::string::npos
    );
}

void request_framing() {
    using namespace gungnir::http;

    const auto valid =
        wire::parse_request(
            "GET / HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "\r\n"
        );

    assert(valid.path() == "/");

    const auto legacy =
        wire::parse_request(
            "GET / HTTP/1.0\r\n"
            "\r\n"
        );

    assert(legacy.path() == "/");

    assert(
        throws_invalid_argument(
            [] {
                static_cast<void>(
                    wire::parse_request(
                        "GET / HTTP/1.1\r\n"
                        "\r\n"
                    )
                );
            }
        )
    );

    assert(
        throws_invalid_argument(
            [] {
                static_cast<void>(
                    wire::parse_request(
                        "GET / HTTP/1.1\r\n"
                        "Host: example.com\r\n"
                        "Host: evil.example\r\n"
                        "\r\n"
                    )
                );
            }
        )
    );

    assert(
        throws_invalid_argument(
            [] {
                static_cast<void>(
                    wire::parse_request(
                        "POST / HTTP/1.1\r\n"
                        "Host: example.com\r\n"
                        "Content-Length: 1\r\n"
                        "Content-Length: 1\r\n"
                        "\r\n"
                        "x"
                    )
                );
            }
        )
    );

    assert(
        throws_invalid_argument(
            [] {
                static_cast<void>(
                    wire::parse_request(
                        "POST / HTTP/1.1\r\n"
                        "Host: example.com\r\n"
                        "Transfer-Encoding: identity\r\n"
                        "\r\n"
                    )
                );
            }
        )
    );

    assert(
        throws_invalid_argument(
            [] {
                static_cast<void>(
                    wire::parse_request(
                        "GET / HTTP/1.1\r\n"
                        "Host : example.com\r\n"
                        "\r\n"
                    )
                );
            }
        )
    );
}

void host_boundary() {
    using namespace gungnir;

    auto middleware =
        http::host_validation(
            {"Example.COM"}
        );

    Request allowed{
        http::Method::get,
        "/"
    };

    allowed.set_header(
        "Host",
        "EXAMPLE.com.:443"
    );

    assert(
        run(
            middleware,
            allowed
        ).status() == 200
    );

    Request denied{
        http::Method::get,
        "/"
    };

    denied.set_header(
        "Host",
        "example.com.evil"
    );

    assert(
        run(
            middleware,
            denied
        ).status() == 400
    );

    assert(
        throws_invalid_argument(
            [] {
                static_cast<void>(
                    gungnir::http::
                        host_validation(
                            {"bad host"}
                        )
                );
            }
        )
    );
}

void proxy_boundary() {
    using namespace gungnir;

    auto middleware =
        http::trusted_proxies(
            http::TrustedProxyOptions{
                .proxies = {
                    "10.0.0.2",
                    "10.0.0.3"
                }
            }
        );

    Request request{
        http::Method::get,
        "/"
    };

    request.client_ip(
        "10.0.0.3"
    );

    request.set_header(
        "X-Forwarded-For",
        "198.51.100.9, 10.0.0.2"
    );

    request.set_header(
        "X-Forwarded-Proto",
        "http, https"
    );

    const auto response =
        run(
            middleware,
            request,
            [](
                Request& current
            ) -> Task<Response> {
                co_return
                    Response::text(
                        std::string{
                            current.client_ip()
                        } +
                        (
                            current.secure()
                                ? ":https"
                                : ":http"
                        )
                    );
            }
        );

    assert(
        response.body() ==
        "198.51.100.9:https"
    );

    Request attacker{
        http::Method::get,
        "/"
    };

    attacker.client_ip(
        "203.0.113.10"
    );

    attacker.set_header(
        "X-Forwarded-For",
        "1.2.3.4"
    );

    attacker.set_header(
        "X-Forwarded-Proto",
        "https"
    );

    const auto ignored =
        run(
            middleware,
            attacker,
            [](
                Request& current
            ) -> Task<Response> {
                co_return
                    Response::text(
                        std::string{
                            current.client_ip()
                        } +
                        (
                            current.secure()
                                ? ":https"
                                : ":http"
                        )
                    );
            }
        );

    assert(
        ignored.body() ==
        "203.0.113.10:http"
    );
}

void cors_boundary() {
    using namespace gungnir;

    auto middleware =
        http::cors(
            http::CorsOptions{
                .allow_origin =
                    "https://app.example",
                .allow_methods =
                    "GET, POST, OPTIONS",
                .allow_headers =
                    "Content-Type, X-CSRF-Token",
                .allow_credentials = true,
                .max_age = 300s
            }
        );

    Request preflight{
        http::Method::options,
        "/resource"
    };

    preflight.set_header(
        "Origin",
        "https://app.example"
    );

    preflight.set_header(
        "Access-Control-Request-Method",
        "POST"
    );

    preflight.set_header(
        "Access-Control-Request-Headers",
        "Content-Type, X-CSRF-Token"
    );

    bool called = false;

    const auto response =
        run(
            middleware,
            preflight,
            [&](
                Request&
            ) -> Task<Response> {
                called = true;
                co_return
                    Response::text(
                        "downstream"
                    );
            }
        );

    assert(!called);
    assert(response.status() == 204);
    assert(
        response.header(
            "access-control-allow-origin"
        ) == "https://app.example"
    );
    assert(
        response.header(
            "access-control-allow-credentials"
        ) == "true"
    );
    assert(
        response.header(
            "access-control-max-age"
        ) == "300"
    );

    Request denied{
        http::Method::options,
        "/resource"
    };

    denied.set_header(
        "Origin",
        "https://evil.example"
    );

    denied.set_header(
        "Access-Control-Request-Method",
        "POST"
    );

    assert(
        run(
            middleware,
            denied
        ).status() == 403
    );

    assert(
        throws_invalid_argument(
            [] {
                static_cast<void>(
                    gungnir::http::cors(
                        gungnir::http::
                            CorsOptions{
                                .allow_origin =
                                    "*",
                                .allow_credentials =
                                    true
                            }
                    )
                );
            }
        )
    );
}

void request_id_boundary() {
    using namespace gungnir;

    auto middleware =
        http::request_id();

    Request invalid{
        http::Method::get,
        "/"
    };

    invalid.set_header(
        "X-Request-ID",
        std::string(129, 'a')
    );

    const auto generated =
        run(
            middleware,
            invalid
        );

    assert(
        generated.header(
            "x-request-id"
        ).starts_with(
            "gungnir-"
        )
    );

    assert(
        generated.header(
            "x-request-id"
        ).size() == 40
    );

    Request valid{
        http::Method::get,
        "/"
    };

    valid.set_header(
        "X-Request-ID",
        "client-id_1"
    );

    const auto preserved =
        run(
            middleware,
            valid
        );

    assert(
        preserved.header(
            "x-request-id"
        ) == "client-id_1"
    );
}

void security_header_boundary() {
    using namespace gungnir;

    auto middleware =
        http::security_headers();

    Request clear{
        http::Method::get,
        "/"
    };

    const auto clear_response =
        run(
            middleware,
            clear
        );

    assert(
        clear_response.header(
            "strict-transport-security"
        ).empty()
    );

    Request secure{
        http::Method::get,
        "/",
        {},
        {},
        true
    };

    const auto secure_response =
        run(
            middleware,
            secure
        );

    assert(
        secure_response.header(
            "strict-transport-security"
        ) == "max-age=31536000"
    );
}

void rate_limit_boundary() {
    using namespace gungnir;

    assert(
        throws_invalid_argument(
            [] {
                static_cast<void>(
                    http::rate_limit(
                        http::RateLimitOptions{
                            .requests = 0
                        }
                    )
                );
            }
        )
    );

    auto middleware =
        http::rate_limit(
            http::RateLimitOptions{
                .requests = 1,
                .window = 60s,
                .max_clients = 2
            }
        );

    Request first{
        http::Method::get,
        "/"
    };

    first.client_ip(
        "192.0.2.1"
    );

    assert(
        run(
            middleware,
            first
        ).status() == 200
    );

    Request second{
        http::Method::get,
        "/"
    };

    second.client_ip(
        "192.0.2.2"
    );

    assert(
        run(
            middleware,
            second
        ).status() == 200
    );

    Request overflow{
        http::Method::get,
        "/"
    };

    overflow.client_ip(
        "192.0.2.3"
    );

    const auto saturated =
        run(
            middleware,
            overflow
        );

    assert(
        saturated.status() == 429
    );
    assert(
        saturated.header(
            "retry-after"
        ) == "60"
    );

    Request repeated{
        http::Method::get,
        "/"
    };

    repeated.client_ip(
        "192.0.2.1"
    );

    const auto limited =
        run(
            middleware,
            repeated
        );

    assert(
        limited.status() == 429
    );
    assert(
        limited.header(
            "x-ratelimit-remaining"
        ) == "0"
    );
}

} // namespace

int main() {
    header_boundaries();
    cookie_prefixes();
    request_framing();
    host_boundary();
    proxy_boundary();
    cors_boundary();
    request_id_boundary();
    security_header_boundary();
    rate_limit_boundary();
}
