#pragma once
#include <string>
#include <utility>
#include <gungnir/http/request.hpp>
#include <gungnir/routing/router.hpp>
#include <gungnir/testing/response_assertions.hpp>

namespace gungnir::testing {

class Http {
public:
    explicit Http(routing::Router& router) : router_(&router) {}

    [[nodiscard]] http::Response get(std::string target) {
        return send(http::Method::get, std::move(target));
    }

    [[nodiscard]] http::Response post(std::string target, std::string body = {}) {
        return send(http::Method::post, std::move(target), std::move(body));
    }

    [[nodiscard]] http::Response send(http::Method method, std::string target, std::string body = {}) {
        http::Request request{method, std::move(target), std::move(body)};
        return router_->dispatch(request).get();
    }

private:
    routing::Router* router_;
};

} // namespace gungnir::testing
