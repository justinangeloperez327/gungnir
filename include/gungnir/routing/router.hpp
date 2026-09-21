#pragma once

#include <functional>
#include <memory>
#include <string>

#include <gungnir/core/task.hpp>
#include <gungnir/http/method.hpp>
#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>

namespace gungnir::routing {

using Handler = std::function<Task<http::Response>(http::Request&)>;
using SyncHandler = std::function<http::Response(http::Request&)>;
using SimpleHandler = std::function<http::Response()>;

class Router {
public:
    Router();
    ~Router();

    Router(Router&&) noexcept;
    Router& operator=(Router&&) noexcept;

    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    Router& add(http::Method method, std::string path, Handler handler);
    Router& add(http::Method method, std::string path, SyncHandler handler);
    Router& add(http::Method method, std::string path, SimpleHandler handler);

    Router& get(std::string path, Handler handler);
    Router& get(std::string path, SyncHandler handler);
    Router& get(std::string path, SimpleHandler handler);

    Router& post(std::string path, Handler handler);
    Router& post(std::string path, SyncHandler handler);
    Router& post(std::string path, SimpleHandler handler);

    Router& put(std::string path, Handler handler);
    Router& put(std::string path, SyncHandler handler);
    Router& put(std::string path, SimpleHandler handler);

    Router& patch(std::string path, Handler handler);
    Router& patch(std::string path, SyncHandler handler);
    Router& patch(std::string path, SimpleHandler handler);

    Router& delete_(std::string path, Handler handler);
    Router& delete_(std::string path, SyncHandler handler);
    Router& delete_(std::string path, SimpleHandler handler);

    Router& remove(std::string path, Handler handler);
    Router& remove(std::string path, SyncHandler handler);
    Router& remove(std::string path, SimpleHandler handler);

    [[nodiscard]] Task<http::Response> dispatch(http::Request& request) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::routing
