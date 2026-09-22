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
#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>

namespace gungnir::routing {

using Handler = std::function<Task<http::Response>(http::Request&)>;
using SyncHandler = std::function<http::Response(http::Request&)>;
using SimpleHandler = std::function<http::Response()>;

class Router;

class RouteRegistration {
public:
    RouteRegistration() = default;
    RouteRegistration& middleware(http::MiddlewareHandler handler);
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
    RouteRegistration get(std::string path, Handler handler);
    RouteRegistration get(std::string path, SyncHandler handler);
    RouteRegistration get(std::string path, SimpleHandler handler);
    RouteRegistration post(std::string path, Handler handler);
    RouteRegistration post(std::string path, SyncHandler handler);
    RouteRegistration post(std::string path, SimpleHandler handler);
private:
    Router* router_;
    std::string prefix_;
    std::vector<http::MiddlewareHandler> middleware_;
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

    Router& use(http::MiddlewareHandler middleware);
    [[nodiscard]] RouteGroup group(std::string prefix);
    [[nodiscard]] std::string url(std::string_view name, const std::unordered_map<std::string, std::string>& parameters = {}) const;
    [[nodiscard]] Task<http::Response> dispatch(http::Request& request) const;

private:
    friend class RouteRegistration;
    void add_middleware(std::size_t route, http::MiddlewareHandler middleware);
    void set_name(std::size_t route, std::string name);
    void set_constraint(std::size_t route, std::string parameter, std::string expression);
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::routing
