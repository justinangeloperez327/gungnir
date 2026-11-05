#include <cassert>
#include <coroutine>
#include <stdexcept>
#include <string>

#include <gungnir/gungnir.hpp>
#include <gungnir/logging/memory_sink.hpp>
#include <sstream>
#include <iostream>
#include <thread>

namespace {
struct ErrorGate {
    std::coroutine_handle<> continuation;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> value) noexcept { continuation = value; }
    void await_resume() const noexcept {}
};

template <typename T>
T sync_wait(gungnir::Task<T> task) {
    task.run_inline();
    auto awaiter = task.operator co_await();
    return awaiter.await_resume();
}

void extended_errors() {
    using namespace gungnir;
    auto logs = std::make_shared<logging::MemorySink>();
    auto logger = std::make_shared<logging::Logger>(); logger->sink(logs);
    routing::Router router;
    router.exceptions().logger(logger).on_report([](Request&,const auto&) { return false; });
    router.exceptions().on_render([](Request&,const auto&) -> std::optional<Response> { return std::nullopt; });
    router.get("/boom", []() -> Response { throw errors::Error{"<script>private failure</script>"}; });
    router.get("/server", []() -> Response { throw http::HttpException{503,"private service credentials"}; });
    router.get("/async", [](Request&) -> Task<Response> { throw errors::RuntimeError{"async failure"}; co_return Response::text("unreachable"); });
    Request request{http::Method::get,"/boom?secret=do-not-log"}; request.set_header("Accept","application/json");
    request.set_header("Authorization","do-not-log");
    auto response = sync_wait(router.dispatch(request));
    assert(response.status() == 500 && response.body().find("private failure") == String::npos);
    assert(response.header("Cache-Control") == "no-store");
    assert(logs->records().size() == 1);
    assert(logs->records().front().message == "<script>private failure</script>");
    const auto first_log = logs->records().front();
    for (const auto& [key,value] : first_log.context) assert(value.find("do-not-log") == String::npos);
    router.exceptions().debug(true);
    response = sync_wait(router.dispatch(request));
    const auto debug = Json::parse(response.body());
    assert(debug.get("message")->string().find("private failure") != String::npos);
    assert(debug.get("file") && debug.get("line") && debug.get("trace"));
    request.set_header("Accept","text/html");
    response = sync_wait(router.dispatch(request));
    assert(response.body().find("&lt;script&gt;") != String::npos && response.body().find("<script>") == String::npos);
    assert(!response.header("Content-Security-Policy").empty());
    router.exceptions().debug(false);
    response = sync_wait(router.dispatch(request));
    assert(response.body().find("private failure") == String::npos && response.body().find("Stack frames") == String::npos);
    Request server{http::Method::get,"/server"};
    response = sync_wait(router.dispatch(server));
    assert(response.status() == 503 && response.body().find("credentials") == String::npos);
    Request asynchronous{http::Method::get,"/async"};
    assert(sync_wait(router.dispatch(asynchronous)).status() == 500);

    routing::Router customized;
    customized.exceptions().logger(logger);
    customized.get("/boom", []() -> Response { throw errors::RuntimeError{"handled"}; });
    int reports = 0;
    customized.exceptions().report<errors::RuntimeError>([&](const auto&, Request&) { ++reports; return true; });
    customized.exceptions().render<errors::RuntimeError>([](const auto&, Request&) -> std::optional<Response> { return Response::text("custom unavailable",503); });
    const auto count = logs->records().size();
    Request custom{http::Method::get,"/boom"};
    response = sync_wait(customized.dispatch(custom));
    assert(response.status() == 503 && response.body() == "custom unavailable" && reports == 1 && logs->records().size() == count);
    customized.exceptions().dont_report<errors::RuntimeError>();
    (void)sync_wait(customized.dispatch(custom)); assert(reports == 1);
    customized.exceptions().render<http::NotFoundException>([](const auto&, Request&) -> std::optional<Response> { return Response::text("custom 404",404); });
    Request missing{http::Method::get,"/missing"};
    assert(sync_wait(customized.dispatch(missing)).body() == "custom 404");

    routing::Router broken;
    broken.exceptions().logger(logger).on_report([](Request&, const auto&) -> bool { throw std::runtime_error("reporter failed"); });
    broken.exceptions().on_render([](Request&, const auto&) -> std::optional<Response> { throw std::runtime_error("renderer failed"); });
    broken.get("/boom", []() -> Response { throw std::runtime_error("original secret"); });
    response = sync_wait(broken.dispatch(custom));
    assert(response.status() == 500 && response.body() == "Internal Server Error");
    assert(logs->records().size() == count + 3); // Failed reporter, original error, failed renderer.
    Request broken_missing{http::Method::get,"/missing"};
    assert(sync_wait(broken.dispatch(broken_missing)).status() == 404);

    Application automatic;
    automatic.container().instance<logging::Logger>(logger);
    automatic.router().get("/boom", []() -> Response { throw std::runtime_error("automatic logger"); });
    Request automatically_logged{http::Method::get,"/boom"};
    (void)sync_wait(automatic.router().dispatch(automatically_logged));
    assert(logs->records().back().message == "automatic logger");
    routing::Router fallback;
    fallback.get("/boom", []() -> Response { throw std::runtime_error("fallback logger"); });
    std::ostringstream standard_error;
    auto* previous_buffer = std::clog.rdbuf(standard_error.rdbuf());
    Request fallback_request{http::Method::get,"/boom"};
    const auto fallback_response = sync_wait(fallback.dispatch(fallback_request));
    std::clog.rdbuf(previous_buffer);
    assert(fallback_response.status() == 500 && standard_error.str().find("fallback logger") != String::npos);

    Application production, development;
    for (auto* app : {&production,&development}) {
        app->config().set("app.debug",true);
        app->exceptions().logger(logger);
        app->router().get("/boom", []() -> Response { throw errors::Error{"application private"}; });
    }
    production.config().set("app.env","production"); development.config().set("app.env","local");
    Request prod{http::Method::get,"/boom"}; prod.set_header("Accept","application/json");
    Request dev{http::Method::get,"/boom"}; dev.set_header("Accept","application/json");
    assert(sync_wait(production.router().dispatch(prod)).body().find("application private") == String::npos);
    production.config().set("app.env","prod"); production.exceptions().debug(true);
    assert(sync_wait(production.router().dispatch(prod)).body().find("application private") == String::npos);
    assert(sync_wait(development.router().dispatch(dev)).body().find("application private") != String::npos);
    ErrorGate gate;
    production.router().get("/suspended", [&gate](Request&) -> Task<Response> {
        co_await gate;
        throw errors::Error{"private suspended failure"};
        co_return Response::text("unreachable");
    });
    Request suspended{http::Method::get,"/suspended"}; suspended.set_header("Accept","application/json");
    auto pending = production.router().dispatch(suspended); pending.run_inline();
    assert(gate.continuation && !pending.operator co_await().await_ready());
    std::thread worker{[&gate] { gate.continuation.resume(); }}; worker.join();
    assert(pending.operator co_await().await_ready());
    response = pending.operator co_await().await_resume();
    assert(response.status() == 500 && response.body().find("private suspended failure") == String::npos);
}

} // namespace

int main() {
    using namespace gungnir;
    extended_errors();

    routing::Router router;

    router.get(
        "/validation",
        [](Request& request) -> Response {
            (void) request.validate({
                {"email", "required|email"}
            });

            return Response::text("ok");
        }
    );

    router.get(
        "/auth",
        [](Request&) -> Response {
            throw http::AuthenticationException{};
        }
    );

    router.get(
        "/forbidden",
        [](Request&) -> Response {
            throw http::AuthorizationException{};
        }
    );

    router.get(
        "/missing",
        [](Request&) -> Response {
            throw ModelNotFoundError{"users"};
        }
    );

    router.get(
        "/boom",
        [](Request&) -> Response {
            throw std::runtime_error(
                "secret internal failure"
            );
        }
    );

    Request validation_request{
        http::Method::get,
        "/validation"
    };

    validation_request.set_header(
        "Accept",
        "application/json"
    );

    const auto validation_response = sync_wait(
        router.dispatch(validation_request)
    );

    assert(validation_response.status() == 422);
    assert(
        validation_response.body().find(
            "\"errors\""
        ) != std::string::npos
    );
    assert(
        validation_response.body().find(
            "\"email\""
        ) != std::string::npos
    );

    Request auth_request{
        http::Method::get,
        "/auth"
    };

    assert(
        sync_wait(
            router.dispatch(auth_request)
        ).status() == 401
    );

    Request forbidden_request{
        http::Method::get,
        "/forbidden"
    };

    assert(
        sync_wait(
            router.dispatch(forbidden_request)
        ).status() == 403
    );

    Request missing_request{
        http::Method::get,
        "/missing"
    };

    assert(
        sync_wait(
            router.dispatch(missing_request)
        ).status() == 404
    );

    Request boom_request{
        http::Method::get,
        "/boom"
    };

    boom_request.set_header(
        "Accept",
        "application/json"
    );

    const auto boom_response = sync_wait(
        router.dispatch(boom_request)
    );

    assert(boom_response.status() == 500);
    assert(
        boom_response.body().find(
            "secret internal failure"
        ) == std::string::npos
    );
    assert(
        boom_response.body().find(
            "Internal Server Error"
        ) != std::string::npos
    );

    return 0;
}
