#pragma once

#include <stdexcept>
#include <string>

#include <gungnir/http/request.hpp>
#include <gungnir/routing/router.hpp>

namespace gungnir::testing {

class TestResponse {
public:
    explicit TestResponse(http::Response response):response_(std::move(response)){}
    [[nodiscard]] const http::Response& response()const noexcept{return response_;}
    TestResponse& assert_status(int expected){if(response_.status()!=expected)throw std::runtime_error("Unexpected response status");return *this;}
    TestResponse& assert_ok(){return assert_status(200);}
    TestResponse& assert_body_contains(std::string_view value){if(response_.body().find(value)==std::string_view::npos)throw std::runtime_error("Response body assertion failed");return *this;}
private:
    http::Response response_;
};

class Application {
public:
    explicit Application(routing::Router& router):router_(&router){}
    [[nodiscard]] Task<TestResponse> get(std::string target){
        http::Request request{http::Method::get,std::move(target)};
        co_return TestResponse{co_await router_->dispatch(request)};
    }
    [[nodiscard]] Task<TestResponse> post(std::string target,std::string body={}){
        http::Request request{http::Method::post,std::move(target),std::move(body)};
        co_return TestResponse{co_await router_->dispatch(request)};
    }
private:
    routing::Router* router_;
};

} // namespace gungnir::testing
