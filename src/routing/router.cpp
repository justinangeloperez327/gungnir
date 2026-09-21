#include <gungnir/routing/router.hpp>

#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gungnir::routing {

namespace {

struct RouteEntry {
    http::Method method;
    std::string path;
    Handler handler;
};

Task<http::Response> sync_to_task(
    SyncHandler handler,
    http::Request& request
) {
    co_return handler(request);
}

Task<http::Response> simple_to_task(SimpleHandler handler) {
    co_return handler();
}

std::vector<std::string_view> split_path(std::string_view path) {
    std::vector<std::string_view> segments;

    std::size_t start = 0;
    while (start < path.size()) {
        while (start < path.size() && path[start] == '/') {
            ++start;
        }

        if (start >= path.size()) {
            break;
        }

        const auto end = path.find('/', start);
        if (end == std::string_view::npos) {
            segments.push_back(path.substr(start));
            break;
        }

        segments.push_back(path.substr(start, end - start));
        start = end + 1;
    }

    return segments;
}

bool match_path(
    std::string_view pattern,
    std::string_view actual,
    http::Request::Parameters& parameters
) {
    const auto pattern_segments = split_path(pattern);
    const auto actual_segments = split_path(actual);

    if (pattern_segments.size() != actual_segments.size()) {
        return false;
    }

    for (std::size_t index = 0; index < pattern_segments.size(); ++index) {
        const auto expected = pattern_segments[index];
        const auto found = actual_segments[index];

        if (
            expected.size() >= 3 &&
            expected.front() == '{' &&
            expected.back() == '}'
        ) {
            const auto name = expected.substr(1, expected.size() - 2);
            if (name.empty()) {
                return false;
            }

            parameters.insert_or_assign(
                std::string{name},
                std::string{found}
            );
            continue;
        }

        if (expected != found) {
            return false;
        }
    }

    return true;
}

} // namespace

class Router::Impl {
public:
    std::vector<RouteEntry> routes;
};

Router::Router() : impl_(std::make_unique<Impl>()) {}
Router::~Router() = default;
Router::Router(Router&&) noexcept = default;
Router& Router::operator=(Router&&) noexcept = default;

Router& Router::add(http::Method method, std::string path, Handler handler) {
    impl_->routes.push_back(RouteEntry{method, std::move(path), std::move(handler)});
    return *this;
}

Router& Router::add(http::Method method, std::string path, SyncHandler handler) {
    return add(
        method,
        std::move(path),
        [handler = std::move(handler)](http::Request& request) mutable {
            return sync_to_task(handler, request);
        }
    );
}

Router& Router::add(http::Method method, std::string path, SimpleHandler handler) {
    return add(
        method,
        std::move(path),
        [handler = std::move(handler)](http::Request&) mutable {
            return simple_to_task(handler);
        }
    );
}

Router& Router::get(std::string path, Handler handler) {
    return add(http::Method::get, std::move(path), std::move(handler));
}

Router& Router::get(std::string path, SyncHandler handler) {
    return add(http::Method::get, std::move(path), std::move(handler));
}

Router& Router::get(std::string path, SimpleHandler handler) {
    return add(http::Method::get, std::move(path), std::move(handler));
}

Router& Router::post(std::string path, Handler handler) {
    return add(http::Method::post, std::move(path), std::move(handler));
}

Router& Router::post(std::string path, SyncHandler handler) {
    return add(http::Method::post, std::move(path), std::move(handler));
}

Router& Router::post(std::string path, SimpleHandler handler) {
    return add(http::Method::post, std::move(path), std::move(handler));
}

Router& Router::put(std::string path, Handler handler) {
    return add(http::Method::put, std::move(path), std::move(handler));
}

Router& Router::put(std::string path, SyncHandler handler) {
    return add(http::Method::put, std::move(path), std::move(handler));
}

Router& Router::put(std::string path, SimpleHandler handler) {
    return add(http::Method::put, std::move(path), std::move(handler));
}

Router& Router::patch(std::string path, Handler handler) {
    return add(http::Method::patch, std::move(path), std::move(handler));
}

Router& Router::patch(std::string path, SyncHandler handler) {
    return add(http::Method::patch, std::move(path), std::move(handler));
}

Router& Router::patch(std::string path, SimpleHandler handler) {
    return add(http::Method::patch, std::move(path), std::move(handler));
}

Router& Router::delete_(std::string path, Handler handler) {
    return add(http::Method::delete_, std::move(path), std::move(handler));
}

Router& Router::delete_(std::string path, SyncHandler handler) {
    return add(http::Method::delete_, std::move(path), std::move(handler));
}

Router& Router::delete_(std::string path, SimpleHandler handler) {
    return add(http::Method::delete_, std::move(path), std::move(handler));
}

Router& Router::remove(std::string path, Handler handler) {
    return delete_(std::move(path), std::move(handler));
}

Router& Router::remove(std::string path, SyncHandler handler) {
    return delete_(std::move(path), std::move(handler));
}

Router& Router::remove(std::string path, SimpleHandler handler) {
    return delete_(std::move(path), std::move(handler));
}

Task<http::Response> Router::dispatch(http::Request& request) const {
    request.clear_route_parameters();

    for (const auto& route : impl_->routes) {
        if (route.method != request.method()) {
            continue;
        }

        http::Request::Parameters parameters;

        if (!match_path(route.path, request.path(), parameters)) {
            continue;
        }

        request.clear_route_parameters();
        for (auto& [name, value] : parameters) {
            request.set_route_parameter(
                std::move(name),
                std::move(value)
            );
        }

        co_return co_await route.handler(request);
    }

    request.clear_route_parameters();
    co_return http::Response::not_found();
}

} // namespace gungnir::routing
