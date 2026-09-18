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

class Router {
public:
    Router();
    ~Router();

    Router(Router&&) noexcept;
    Router& operator=(Router&&) noexcept;

    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    Router& add(http::Method method, std::string path, Handler handler);
    Router& get(std::string path, Handler handler);
    Router& post(std::string path, Handler handler);
    Router& put(std::string path, Handler handler);
    Router& patch(std::string path, Handler handler);
    Router& remove(std::string path, Handler handler);

    [[nodiscard]] Task<http::Response> dispatch(http::Request& request) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::routing
