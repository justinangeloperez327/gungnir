#include <cassert>

#include <gungnir/core/application.hpp>
#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>

int main() {
    gungnir::Application app;
    assert(!app.is_booted());

    app.boot();
    assert(app.is_booted());

    app.shutdown();
    assert(!app.is_booted());

    gungnir::http::Request request{gungnir::http::Method::get, "/health"};
    assert(request.method() == gungnir::http::Method::get);
    assert(request.path() == "/health");

    request.set_header("accept", "application/json");
    assert(request.header("accept") == "application/json");

    auto response = gungnir::http::Response::text("ok");
    assert(response.status() == 200);
    assert(response.body() == "ok");
    assert(response.header("content-type") == "text/plain; charset=utf-8");

    return 0;
}
