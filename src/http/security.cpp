#include <gungnir/http/security.hpp>

#include <gungnir/security/security.hpp>

#include <atomic>
#include <chrono>
#include <cctype>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace gungnir::http {
namespace {

std::atomic_uint64_t ids{1};

bool csrf_safe_method(
    Method method
) noexcept {
    return
        method == Method::get ||
        method == Method::head ||
        method == Method::options;
}

std::string_view presented_csrf_token(
    Request& request,
    const CsrfOptions& options
) {
    const auto header =
        request.header(
            options.header_name
        );

    if (!header.empty()) {
        return header;
    }

    const auto& form =
        request.form();

    const auto found =
        form.find(
            options.form_field
        );

    return
        found == form.end()
            ? std::string_view{}
            : std::string_view{
                found->second
              };
}

void validate_csrf_options(
    const CsrfOptions& options
) {
    if (options.session_key.empty()) {
        throw std::invalid_argument(
            "CSRF session key cannot be empty"
        );
    }

    if (options.header_name.empty()) {
        throw std::invalid_argument(
            "CSRF header name cannot be empty"
        );
    }

    if (options.form_field.empty()) {
        throw std::invalid_argument(
            "CSRF form field cannot be empty"
        );
    }
}

std::string_view trim(
    std::string_view value
) {
    while (
        !value.empty() &&
        std::isspace(
            static_cast<unsigned char>(
                value.front()
            )
        ) != 0
    ) {
        value.remove_prefix(1);
    }

    while (
        !value.empty() &&
        std::isspace(
            static_cast<unsigned char>(
                value.back()
            )
        ) != 0
    ) {
        value.remove_suffix(1);
    }

    return value;
}

std::string forwarded_for_client(
    std::string_view value
) {
    const auto comma =
        value.find(',');

    return std::string{
        trim(
            comma == std::string_view::npos
                ? value
                : value.substr(0, comma)
        )
    };
}

} // namespace

MiddlewareHandler cors(CorsOptions options) {
    return [options = std::move(options)](Request& request, Next next) -> Task<Response> {
        auto response = co_await next(request);
        response.header("access-control-allow-origin", options.allow_origin)
            .header("access-control-allow-methods", options.allow_methods)
            .header("access-control-allow-headers", options.allow_headers);
        co_return response;
    };
}

MiddlewareHandler security_headers() {
    return [](Request& request, Next next) -> Task<Response> {
        auto response = co_await next(request);
        response.header("x-content-type-options", "nosniff")
            .header("x-frame-options", "DENY")
            .header("referrer-policy", "strict-origin-when-cross-origin")
            .header("content-security-policy", "default-src 'self'");
        co_return response;
    };
}

MiddlewareHandler body_limit(std::size_t bytes) {
    return [bytes](Request& request, Next next) -> Task<Response> {
        if (request.body().size() > bytes) co_return Response::text("Payload Too Large", 413);
        co_return co_await next(request);
    };
}

MiddlewareHandler host_validation(std::unordered_set<std::string> hosts) {
    return [hosts = std::move(hosts)](Request& request, Next next) -> Task<Response> {
        const auto host = std::string{request.header("host")};
        if (!hosts.empty() && !hosts.contains(host)) co_return Response::text("Invalid Host", 400);
        co_return co_await next(request);
    };
}

MiddlewareHandler request_id() {
    return [](Request& request, Next next) -> Task<Response> {
        auto id = std::string{request.header("x-request-id")};
        if (id.empty()) id = "gungnir-" + std::to_string(ids.fetch_add(1));
        auto response = co_await next(request);
        response.header("x-request-id", id);
        co_return response;
    };
}

MiddlewareHandler request_timing() {
    return [](Request& request, Next next) -> Task<Response> {
        const auto start = std::chrono::steady_clock::now();
        auto response = co_await next(request);
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();
        response.header("server-timing", "app;dur=" + std::to_string(elapsed / 1000.0));
        co_return response;
    };
}

MiddlewareHandler trusted_proxies(
    TrustedProxyOptions options
) {
    return [
        options = std::move(options)
    ](
        Request& request,
        Next next
    ) -> Task<Response> {
        const auto peer =
            std::string{
                request.client_ip()
            };

        if (
            !peer.empty() &&
            options.proxies.contains(peer)
        ) {
            auto forwarded =
                forwarded_for_client(
                    request.header(
                        "x-forwarded-for"
                    )
                );

            if (forwarded.empty()) {
                forwarded =
                    std::string{
                        trim(
                            request.header(
                                "x-real-ip"
                            )
                        )
                    };
            }

            if (!forwarded.empty()) {
                request.client_ip(
                    std::move(forwarded)
                );
            }
        }

        co_return co_await next(request);
    };
}

MiddlewareHandler rate_limit(RateLimitOptions options) {
    struct Bucket { std::size_t count{}; std::chrono::steady_clock::time_point reset{}; };
    auto buckets = std::make_shared<std::unordered_map<std::string, Bucket>>();
    auto mutex = std::make_shared<std::mutex>();
    return [options, buckets, mutex](Request& request, Next next) -> Task<Response> {
        auto key = std::string{request.client_ip()};
        if (key.empty()) {
            key = "unknown-client";
        }
        const auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard lock{*mutex};
            auto& bucket = (*buckets)[key];
            if (bucket.reset <= now) { bucket.count = 0; bucket.reset = now + options.window; }
            if (++bucket.count > options.requests) co_return Response::text("Too Many Requests", 429);
        }
        co_return co_await next(request);
    };
}

std::string_view csrf_token(
    Request& request,
    const CsrfOptions& options
) {
    validate_csrf_options(
        options
    );

    if (!request.has_session()) {
        throw std::logic_error(
            "CSRF protection requires session middleware to run first"
        );
    }

    auto& current =
        request.session();

    if (
        !current.has(
            options.session_key
        ) ||
        current.get(
            options.session_key
        ).empty()
    ) {
        current.put(
            options.session_key,
            security::random_token()
        );
    }

    return current.get(
        options.session_key
    );
}

MiddlewareHandler csrf(
    CsrfOptions options
) {
    validate_csrf_options(
        options
    );

    return [
        options = std::move(options)
    ](
        Request& request,
        Next next
    ) -> Task<Response> {
        if (!request.has_session()) {
            throw std::logic_error(
                "CSRF protection requires session middleware to run first"
            );
        }

        auto& current =
            request.session();

        const auto initial_session_id =
            std::string{
                current.id()
            };

        if (
            csrf_safe_method(
                request.method()
            )
        ) {
            static_cast<void>(
                csrf_token(
                    request,
                    options
                )
            );
        } else {
            const auto expected =
                current.get(
                    options.session_key
                );

            const auto presented =
                presented_csrf_token(
                    request,
                    options
                );

            if (
                expected.empty() ||
                presented.empty() ||
                !security::
                    constant_time_equal(
                        expected,
                        presented
                    )
            ) {
                co_return Response::text(
                    "Page Expired",
                    419
                );
            }
        }

        auto response =
            co_await next(request);

        if (
            current.id() !=
                initial_session_id ||
            current.regenerated()
        ) {
            current.put(
                options.session_key,
                security::random_token()
            );
        }

        co_return response;
    };
}

} // namespace gungnir::http
