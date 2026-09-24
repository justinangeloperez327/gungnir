#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <gungnir/core/task.hpp>
#include <gungnir/http/exception_handler.hpp>
#include <gungnir/http/method.hpp>
#include <gungnir/http/middleware.hpp>
#include <gungnir/http/middleware_registry.hpp>
#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>

namespace gungnir {
class Container;
}

namespace gungnir::routing {

using Handler = std::function<Task<http::Response>(http::Request&)>;
using SyncHandler = std::function<http::Response(http::Request&)>;
using SimpleHandler = std::function<http::Response()>;

class Router;

class RouteRegistration {
public:
    RouteRegistration() = default;
    RouteRegistration& middleware(http::MiddlewareHandler handler);
    RouteRegistration& middleware(std::string alias);
    RouteRegistration& name(std::string value);
    RouteRegistration& where(std::string parameter, std::string expression);
    RouteRegistration& where_number(std::string parameter);
    RouteRegistration& where_uuid(std::string parameter);

    template <typename MiddlewareType>
    RouteRegistration& middleware();

private:
    friend class Router;
    RouteRegistration(Router* router, std::size_t index) noexcept : router_(router), index_(index) {}
    Router* router_{nullptr};
    std::size_t index_{0};
};

class RouteGroup {
public:
    RouteGroup(Router& router, std::string prefix);
    RouteGroup& middleware(http::MiddlewareHandler handler);
    RouteGroup& middleware(std::string alias);
    RouteGroup& middleware_group(std::string group);
    RouteRegistration get(std::string path, Handler handler);
    RouteRegistration get(std::string path, SyncHandler handler);
    RouteRegistration get(std::string path, SimpleHandler handler);
    RouteRegistration post(std::string path, Handler handler);
    RouteRegistration post(std::string path, SyncHandler handler);
    RouteRegistration post(std::string path, SimpleHandler handler);
    RouteRegistration put(std::string path, Handler handler);
    RouteRegistration put(std::string path, SyncHandler handler);
    RouteRegistration put(std::string path, SimpleHandler handler);
    RouteRegistration patch(std::string path, Handler handler);
    RouteRegistration patch(std::string path, SyncHandler handler);
    RouteRegistration patch(std::string path, SimpleHandler handler);
    RouteRegistration delete_(std::string path, Handler handler);
    RouteRegistration delete_(std::string path, SyncHandler handler);
    RouteRegistration delete_(std::string path, SimpleHandler handler);
    RouteRegistration options(std::string path, Handler handler);
    RouteRegistration head(std::string path, Handler handler);
private:
    Router* router_;
    std::string prefix_;
    std::vector<http::MiddlewareHandler> middleware_;
    std::vector<std::string> middleware_aliases_;
    std::vector<std::string> middleware_groups_;
    std::string path(std::string_view value) const;
    RouteRegistration apply(RouteRegistration registration);
};

class Router {
public:
    Router();
    ~Router();
    Router(Router&&) noexcept;
    Router& operator=(Router&&) noexcept;
    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    RouteRegistration add(http::Method method, std::string path, Handler handler);
    RouteRegistration add(http::Method method, std::string path, SyncHandler handler);
    RouteRegistration add(http::Method method, std::string path, SimpleHandler handler);
    RouteRegistration get(std::string path, Handler handler);
    RouteRegistration get(std::string path, SyncHandler handler);
    RouteRegistration get(std::string path, SimpleHandler handler);
    RouteRegistration post(std::string path, Handler handler);
    RouteRegistration post(std::string path, SyncHandler handler);
    RouteRegistration post(std::string path, SimpleHandler handler);
    RouteRegistration put(std::string path, Handler handler);
    RouteRegistration put(std::string path, SyncHandler handler);
    RouteRegistration put(std::string path, SimpleHandler handler);
    RouteRegistration patch(std::string path, Handler handler);
    RouteRegistration patch(std::string path, SyncHandler handler);
    RouteRegistration patch(std::string path, SimpleHandler handler);
    RouteRegistration delete_(std::string path, Handler handler);
    RouteRegistration delete_(std::string path, SyncHandler handler);
    RouteRegistration delete_(std::string path, SimpleHandler handler);
    RouteRegistration remove(std::string path, Handler handler);
    RouteRegistration remove(std::string path, SyncHandler handler);
    RouteRegistration remove(std::string path, SimpleHandler handler);
    RouteRegistration options(std::string path, Handler handler);
    RouteRegistration options(std::string path, SyncHandler handler);
    RouteRegistration options(std::string path, SimpleHandler handler);
    RouteRegistration head(std::string path, Handler handler);
    RouteRegistration head(std::string path, SyncHandler handler);
    RouteRegistration head(std::string path, SimpleHandler handler);

    Router& fallback(Handler handler);
    Router& fallback(SyncHandler handler);
    Router& fallback(SimpleHandler handler);
    [[nodiscard]] bool has(std::string_view name) const noexcept;
    [[nodiscard]] std::size_t route_count() const noexcept;

    Router& use(http::MiddlewareHandler middleware);
    Router& middleware_registry(http::MiddlewareRegistry& registry);
    Router& service_container(Container& container) noexcept;
    [[nodiscard]] RouteGroup group(std::string prefix);
    [[nodiscard]] std::string url(std::string_view name, const std::unordered_map<std::string, std::string>& parameters = {}) const;
    [[nodiscard]] Task<http::Response> dispatch(http::Request& request) const;

private:
    friend class RouteRegistration;
    friend class RouteGroup;
    void add_middleware(std::size_t route, http::MiddlewareHandler middleware);
    void add_middleware(std::size_t route, std::string alias);
    void set_name(std::size_t route, std::string name);
    void set_constraint(std::size_t route, std::string parameter, std::string expression);
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::routing
