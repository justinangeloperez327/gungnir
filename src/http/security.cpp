#include <gungnir/http/security.hpp>

#include <gungnir/security/security.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <charconv>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace gungnir::http {
namespace {

bool csrf_safe_method(
    Method method
) noexcept {
    return
        method == Method::get ||
        method == Method::head ||
        method == Method::options;
}

std::string_view trim(
    std::string_view value
) noexcept {
    while (
        !value.empty() &&
        (
            value.front() == ' ' ||
            value.front() == '\t'
        )
    ) {
        value.remove_prefix(1);
    }

    while (
        !value.empty() &&
        (
            value.back() == ' ' ||
            value.back() == '\t'
        )
    ) {
        value.remove_suffix(1);
    }

    return value;
}

std::string lowercase(
    std::string_view value
) {
    std::string result{value};

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char character) {
            return static_cast<char>(
                std::tolower(character)
            );
        }
    );

    return result;
}

std::vector<std::string>
split_csv(
    std::string_view value,
    std::size_t maximum = 64
) {
    std::vector<std::string> values;
    std::size_t cursor = 0;

    while (cursor <= value.size()) {
        if (values.size() >= maximum) {
            return {};
        }

        const auto comma =
            value.find(',', cursor);

        const auto end =
            comma == std::string_view::npos
                ? value.size()
                : comma;

        const auto item =
            trim(
                value.substr(
                    cursor,
                    end - cursor
                )
            );

        if (item.empty()) {
            return {};
        }

        values.emplace_back(item);

        if (comma == std::string_view::npos) {
            break;
        }

        cursor = comma + 1;
    }

    return values;
}

bool csv_contains(
    std::string_view csv,
    std::string_view candidate
) {
    const auto expected =
        lowercase(trim(candidate));

    if (expected.empty()) {
        return false;
    }

    if (trim(csv) == "*") {
        return true;
    }

    for (const auto& item : split_csv(csv)) {
        if (lowercase(item) == expected) {
            return true;
        }
    }

    return false;
}

bool valid_port(
    std::string_view value
) noexcept {
    if (value.empty()) {
        return false;
    }

    unsigned port = 0;
    const auto parsed =
        std::from_chars(
            value.data(),
            value.data() + value.size(),
            port
        );

    return
        parsed.ec == std::errc{} &&
        parsed.ptr == value.data() + value.size() &&
        port > 0 &&
        port <= 65535;
}

std::optional<std::string>
canonical_host(
    std::string_view input
) {
    input = trim(input);

    if (
        input.empty() ||
        !security::valid_header_value(input) ||
        input.find_first_of("/\\@") !=
            std::string_view::npos
    ) {
        return std::nullopt;
    }

    std::string_view host = input;

    if (input.front() == '[') {
        const auto close = input.find(']');

        if (
            close == std::string_view::npos ||
            close <= 1
        ) {
            return std::nullopt;
        }

        host = input.substr(1, close - 1);

        const auto suffix =
            input.substr(close + 1);

        if (
            !suffix.empty() &&
            (
                suffix.front() != ':' ||
                !valid_port(
                    suffix.substr(1)
                )
            )
        ) {
            return std::nullopt;
        }

        if (
            !std::all_of(
                host.begin(),
                host.end(),
                [](unsigned char character) {
                    return
                        std::isxdigit(character) != 0 ||
                        character == ':' ||
                        character == '.';
                }
            )
        ) {
            return std::nullopt;
        }

        return lowercase(host);
    }

    const auto first_colon = input.find(':');

    if (
        first_colon != std::string_view::npos &&
        input.find(':', first_colon + 1) !=
            std::string_view::npos
    ) {
        // IPv6 Host values must use bracket notation.
        return std::nullopt;
    }

    if (first_colon != std::string_view::npos) {
        if (
            !valid_port(
                input.substr(first_colon + 1)
            )
        ) {
            return std::nullopt;
        }

        host = input.substr(0, first_colon);
    }

    if (
        host.size() > 253 ||
        host.empty()
    ) {
        return std::nullopt;
    }

    if (host.back() == '.') {
        host.remove_suffix(1);
    }

    if (host.empty()) {
        return std::nullopt;
    }

    std::size_t cursor = 0;

    while (cursor <= host.size()) {
        const auto dot =
            host.find('.', cursor);

        const auto end =
            dot == std::string_view::npos
                ? host.size()
                : dot;

        const auto label =
            host.substr(
                cursor,
                end - cursor
            );

        if (
            label.empty() ||
            label.size() > 63 ||
            label.front() == '-' ||
            label.back() == '-' ||
            !std::all_of(
                label.begin(),
                label.end(),
                [](unsigned char character) {
                    return
                        std::isalnum(character) != 0 ||
                        character == '-';
                }
            )
        ) {
            return std::nullopt;
        }

        if (dot == std::string_view::npos) {
            break;
        }

        cursor = dot + 1;
    }

    return lowercase(host);
}

bool valid_request_id(
    std::string_view value
) noexcept {
    if (
        value.empty() ||
        value.size() > 128 ||
        !security::valid_header_value(value)
    ) {
        return false;
    }

    return std::all_of(
        value.begin(),
        value.end(),
        [](unsigned char character) {
            return
                std::isalnum(character) != 0 ||
                character == '-' ||
                character == '_' ||
                character == '.' ||
                character == ':' ||
                character == '/';
        }
    );
}

std::optional<std::string>
cors_origin(
    const CorsOptions& options,
    std::string_view presented
) {
    if (presented.empty()) {
        return std::nullopt;
    }

    if (!options.allow_origins.empty()) {
        if (
            options.allow_origins.contains(
                std::string{presented}
            )
        ) {
            return std::string{presented};
        }

        return std::nullopt;
    }

    if (options.allow_origin == "*") {
        return std::string{"*"};
    }

    if (options.allow_origin == presented) {
        return std::string{presented};
    }

    return std::nullopt;
}

void append_vary(
    Response& response,
    std::string_view value
) {
    const auto existing =
        response.header("vary");

    if (existing.empty()) {
        response.header(
            "vary",
            std::string{value}
        );
        return;
    }

    if (
        lowercase(existing).find(
            lowercase(value)
        ) != std::string::npos
    ) {
        return;
    }

    response.header(
        "vary",
        std::string{existing} +
            ", " +
            std::string{value}
    );
}

void apply_cors_headers(
    Response& response,
    const CorsOptions& options,
    std::string_view origin,
    bool preflight
) {
    response.header(
        "access-control-allow-origin",
        std::string{origin}
    );

    if (options.allow_credentials) {
        response.header(
            "access-control-allow-credentials",
            "true"
        );
    }

    if (origin != "*") {
        append_vary(
            response,
            "Origin"
        );
    }

    if (!preflight) {
        return;
    }

    response
        .header(
            "access-control-allow-methods",
            options.allow_methods
        )
        .header(
            "access-control-allow-headers",
            options.allow_headers
        )
        .header(
            "access-control-max-age",
            std::to_string(
                options.max_age.count()
            )
        );

    append_vary(
        response,
        "Access-Control-Request-Method"
    );
    append_vary(
        response,
        "Access-Control-Request-Headers"
    );
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

    if (
        !security::valid_header_name(
            options.header_name
        )
    ) {
        throw std::invalid_argument(
            "CSRF header name is invalid"
        );
    }

    if (options.form_field.empty()) {
        throw std::invalid_argument(
            "CSRF form field cannot be empty"
        );
    }
}

} // namespace

MiddlewareHandler cors(
    CorsOptions options
) {
    if (
        options.max_age.count() < 0 ||
        !security::valid_header_value(
            options.allow_methods
        ) ||
        !security::valid_header_value(
            options.allow_headers
        ) ||
        !security::valid_header_value(
            options.allow_origin
        )
    ) {
        throw std::invalid_argument(
            "Invalid CORS configuration"
        );
    }

    const bool wildcard =
        options.allow_origins.empty() &&
        options.allow_origin == "*";

    if (
        options.allow_credentials &&
        wildcard
    ) {
        throw std::invalid_argument(
            "Credentialed CORS cannot use a wildcard origin"
        );
    }

    for (const auto& origin :
         options.allow_origins) {
        if (
            origin.empty() ||
            origin == "*" ||
            !security::valid_header_value(
                origin
            )
        ) {
            throw std::invalid_argument(
                "Invalid CORS origin"
            );
        }
    }

    return [
        options = std::move(options)
    ](
        Request& request,
        Next next
    ) -> Task<Response> {
        const auto presented_origin =
            request.header("origin");

        const auto allowed =
            cors_origin(
                options,
                presented_origin
            );

        const bool preflight =
            request.method() ==
                Method::options &&
            !presented_origin.empty() &&
            !request.header(
                "access-control-request-method"
            ).empty();

        if (preflight) {
            if (!allowed) {
                co_return Response::text(
                    "CORS Origin Denied",
                    403
                );
            }

            const auto requested_method =
                request.header(
                    "access-control-request-method"
                );

            if (
                !csv_contains(
                    options.allow_methods,
                    requested_method
                )
            ) {
                co_return Response::text(
                    "CORS Method Denied",
                    403
                );
            }

            const auto requested_headers =
                request.header(
                    "access-control-request-headers"
                );

            if (!requested_headers.empty()) {
                const auto headers =
                    split_csv(
                        requested_headers
                    );

                if (headers.empty()) {
                    co_return Response::text(
                        "CORS Headers Denied",
                        403
                    );
                }

                for (const auto& header :
                     headers) {
                    if (
                        !csv_contains(
                            options.allow_headers,
                            header
                        )
                    ) {
                        co_return Response::text(
                            "CORS Headers Denied",
                            403
                        );
                    }
                }
            }

            auto response =
                Response::no_content();

            apply_cors_headers(
                response,
                options,
                *allowed,
                true
            );

            co_return response;
        }

        auto response =
            co_await next(request);

        if (allowed) {
            apply_cors_headers(
                response,
                options,
                *allowed,
                false
            );
        }

        co_return response;
    };
}

MiddlewareHandler security_headers() {
    return [](
        Request& request,
        Next next
    ) -> Task<Response> {
        auto response =
            co_await next(request);

        response
            .header(
                "x-content-type-options",
                "nosniff"
            )
            .header(
                "x-frame-options",
                "DENY"
            )
            .header(
                "referrer-policy",
                "strict-origin-when-cross-origin"
            )
            .header(
                "content-security-policy",
                "default-src 'self'"
            );

        if (request.secure()) {
            response.header(
                "strict-transport-security",
                "max-age=31536000"
            );
        }

        co_return response;
    };
}

MiddlewareHandler body_limit(
    std::size_t bytes
) {
    return [bytes](
        Request& request,
        Next next
    ) -> Task<Response> {
        if (
            request.body().size() >
            bytes
        ) {
            co_return Response::text(
                "Payload Too Large",
                413
            );
        }

        co_return co_await next(request);
    };
}

MiddlewareHandler host_validation(
    std::unordered_set<std::string> hosts
) {
    std::unordered_set<std::string>
        allowed;

    for (const auto& value : hosts) {
        const auto host =
            canonical_host(value);

        if (!host) {
            throw std::invalid_argument(
                "Invalid allowed Host value"
            );
        }

        allowed.insert(*host);
    }

    return [
        hosts = std::move(allowed)
    ](
        Request& request,
        Next next
    ) -> Task<Response> {
        const auto host =
            canonical_host(
                request.header("host")
            );

        if (
            !host ||
            (
                !hosts.empty() &&
                !hosts.contains(*host)
            )
        ) {
            co_return Response::text(
                "Invalid Host",
                400
            );
        }

        co_return co_await next(request);
    };
}

MiddlewareHandler request_id() {
    return [](
        Request& request,
        Next next
    ) -> Task<Response> {
        auto id =
            std::string{
                request.header(
                    "x-request-id"
                )
            };

        if (!valid_request_id(id)) {
            id =
                "gungnir-" +
                security::random_token(16);
        }

        auto response =
            co_await next(request);

        response.header(
            "x-request-id",
            std::move(id)
        );

        co_return response;
    };
}

MiddlewareHandler request_timing() {
    return [](
        Request& request,
        Next next
    ) -> Task<Response> {
        const auto start =
            std::chrono::steady_clock::now();

        auto response =
            co_await next(request);

        const auto elapsed =
            std::chrono::duration_cast<
                std::chrono::microseconds
            >(
                std::chrono::steady_clock::now() -
                start
            ).count();

        response.header(
            "server-timing",
            "app;dur=" +
                std::to_string(
                    elapsed / 1000.0
                )
        );

        co_return response;
    };
}

MiddlewareHandler trusted_proxies(
    TrustedProxyOptions options
) {
    std::unordered_set<std::string>
        proxies;

    for (const auto& proxy :
         options.proxies) {
        const auto normalized =
            std::string{trim(proxy)};

        if (
            normalized.empty() ||
            normalized.find(',') !=
                std::string::npos ||
            !security::valid_header_value(
                normalized
            )
        ) {
            throw std::invalid_argument(
                "Invalid trusted proxy"
            );
        }

        proxies.insert(normalized);
    }

    options.proxies =
        std::move(proxies);

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
            peer.empty() ||
            !options.proxies.contains(peer)
        ) {
            co_return co_await next(request);
        }

        const auto forwarded =
            request.header(
                "x-forwarded-for"
            );

        if (!forwarded.empty()) {
            const auto chain =
                split_csv(
                    forwarded,
                    32
                );

            if (!chain.empty()) {
                std::string client = peer;

                for (
                    auto iterator =
                        chain.rbegin();
                    iterator !=
                        chain.rend();
                    ++iterator
                ) {
                    if (
                        !options.proxies.contains(
                            client
                        )
                    ) {
                        break;
                    }

                    if (
                        iterator->size() > 255 ||
                        !security::
                            valid_header_value(
                                *iterator
                            )
                    ) {
                        client = peer;
                        break;
                    }

                    client = *iterator;
                }

                if (client != peer) {
                    request.client_ip(
                        std::move(client)
                    );
                }
            }
        } else {
            const auto real_ip =
                trim(
                    request.header(
                        "x-real-ip"
                    )
                );

            if (
                !real_ip.empty() &&
                real_ip.size() <= 255 &&
                security::valid_header_value(
                    real_ip
                )
            ) {
                request.client_ip(
                    std::string{real_ip}
                );
            }
        }

        if (options.trust_forwarded_proto) {
            const auto values =
                split_csv(
                    request.header(
                        "x-forwarded-proto"
                    ),
                    32
                );

            if (!values.empty()) {
                const auto proto =
                    lowercase(
                        values.back()
                    );

                if (proto == "https") {
                    request.secure(true);
                } else if (
                    proto == "http"
                ) {
                    request.secure(false);
                }
            }
        }

        co_return co_await next(request);
    };
}

MiddlewareHandler rate_limit(RateLimitOptions options) {
    if (options.max_clients == 0)
        throw std::invalid_argument("Rate limit client capacity must be positive");
    return rate_limit(options, std::make_shared<MemoryRateLimitStore>(options.max_clients), "local:");
}

MiddlewareHandler rate_limit(RateLimitOptions options, std::shared_ptr<RateLimitStore> store,
    std::string namespace_key) {
    validate_rate_limit(options.requests, options.window);
    if (!store) throw std::invalid_argument("Rate limit store is not configured");
    if (namespace_key.empty()) throw std::invalid_argument("Rate limit namespace cannot be empty");
    return [options, store = std::move(store), namespace_key = std::move(namespace_key)]
        (Request& request, Next next) -> Task<Response> {
        const auto client = request.client_ip().empty() ? std::string{"unknown-client"} : std::string{request.client_ip()};
        // The length delimiter prevents policy/client boundary collisions.
        const auto decision = store->consume(std::to_string(namespace_key.size()) + ":" + namespace_key + client,
            options.requests, options.window);
        if (!decision.allowed) {
            auto response = Response::text("Too Many Requests", 429);
            response.header("retry-after", std::to_string(decision.retry_after.count()))
                .header("x-ratelimit-limit", std::to_string(options.requests))
                .header("x-ratelimit-remaining", "0");
            co_return response;
        }
        auto response = co_await next(request);
        response.header("x-ratelimit-limit", std::to_string(options.requests))
            .header("x-ratelimit-remaining", std::to_string(decision.remaining));
        co_return response;
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
