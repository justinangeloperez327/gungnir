#include <gungnir/routing/router.hpp>

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
    for (const auto& route : impl_->routes) {
        if (route.method == request.method() && route.path == request.path()) {
            co_return co_await route.handler(request);
        }
    }

    co_return http::Response::not_found();
}

} // namespace gungnir::routing
