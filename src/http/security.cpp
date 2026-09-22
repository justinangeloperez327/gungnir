#include <gungnir/http/security.hpp>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace gungnir::http {
namespace {
std::atomic_uint64_t ids{1};
}

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

MiddlewareHandler rate_limit(RateLimitOptions options) {
    struct Bucket { std::size_t count{}; std::chrono::steady_clock::time_point reset{}; };
    auto buckets = std::make_shared<std::unordered_map<std::string, Bucket>>();
    auto mutex = std::make_shared<std::mutex>();
    return [options, buckets, mutex](Request& request, Next next) -> Task<Response> {
        const auto key = std::string{request.header("x-forwarded-for")};
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

} // namespace gungnir::http
