#include <cassert>
#include <chrono>
#include <coroutine>
#include <memory>
#include <stdexcept>
#include <thread>

#include <gungnir/core/task.hpp>
#include <gungnir/core/timer.hpp>
#include <gungnir/database/runtime.hpp>
#include <gungnir/auth/authorize.hpp>
#include <gungnir/http/exception_handler.hpp>
#include <gungnir/http/message.hpp>
#include <gungnir/observability/trace.hpp>
#include <gungnir/testing/testing.hpp>
#include <gungnir/view/engine.hpp>
#include <gungnir/view/runtime.hpp>

namespace {
using namespace gungnir;
struct Pause {
    std::coroutine_handle<> handle;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> value) noexcept { handle = value; }
    void await_resume() const noexcept {}
};
struct HandlePause : Pause {
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> value) noexcept {
        handle = value;
        return std::noop_coroutine();
    }
};
struct NoSuspend {
    bool await_ready() const noexcept { return false; }
    bool await_suspend(std::coroutine_handle<>) const noexcept { return false; }
    void await_resume() const noexcept {}
};
struct ThrowSuspend {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<>) const { throw std::runtime_error("suspend"); }
    void await_resume() const noexcept {}
};
struct Snapshot {
    view::runtime::EngineHandle view;
    database::runtime::ConnectionHandle database;
    std::string trace;
};
Task<Snapshot> child() {
    co_await std::suspend_never{};
    co_await NoSuspend{};
    co_return Snapshot{view::runtime::current(), database::runtime::current(),
                       observability::current_context().trace_id};
}
template <typename Wait>
Task<Snapshot> scoped(view::runtime::EngineHandle engine,
                      database::runtime::ConnectionHandle connection,
                      std::string trace, Wait& pause) {
    auto view_scope = view::runtime::activate(engine);
    auto database_scope = database::runtime::activate(connection);
    auto trace_scope = observability::activate({trace, "span"});
    try { co_await ThrowSuspend{}; }
    catch (const std::runtime_error&) {
        assert(view::runtime::current() == engine);
    }
    co_await pause;
    auto snapshot = co_await child();
    assert(snapshot.view == engine && snapshot.database == connection && snapshot.trace == trace);
    co_return snapshot;
}
void check_cookies() {
    http::Cookie cookie{"session", "abc"};
    assert(http::serialize_cookie(cookie).find("session=abc") == 0);
    const auto rejects = [](const http::Cookie& value) {
        try { (void)http::serialize_cookie(value); }
        catch (const std::invalid_argument&) { return true; }
        return false;
    };
    for (const auto invalid : {std::string{"/\r\nX-Injected: yes"},
                              std::string{"/; Secure"}, std::string{"/\x7f"},
                              std::string{"/\0bad", 5}}) {
        cookie.path = invalid;
        assert(rejects(cookie));
    }
    cookie.path = "/";
    cookie.domain = "example.com\r\nX-Injected: yes";
    assert(rejects(cookie));
    cookie.domain = "bad domain";
    assert(rejects(cookie));
    cookie.domain = ".example.com";
    assert(!rejects(cookie));
    cookie.domain.reset();
    cookie.value = "\"quoted\"";
    assert(!rejects(cookie));
    cookie.name = "bad name";
    assert(rejects(cookie));
    cookie.name = "session";
    cookie.value = "a\nb";
    assert(rejects(cookie));
    cookie.value = "abc";
    cookie.same_site = http::SameSite::none;
    assert(rejects(cookie));
    cookie.secure = true;
    assert(!rejects(cookie));
    cookie.path = "/\r\nX-Injected: yes";
    http::Response response = http::Response::text("ok");
    response.cookie(cookie);
    try { (void)http::wire::serialize_response(response); assert(false); }
    catch (const std::invalid_argument&) {}
}
}

int main() {
    using namespace gungnir;
    check_cookies();
    routing::Router router;
    router.get("/ok", [](http::Request&) -> Task<http::Response> {
        co_return http::Response::text("ok");
    });
    router.get("/async", [](http::Request&) -> Task<http::Response> {
        co_await sleep_for(std::chrono::milliseconds{2});
        co_return http::Response::text("async");
    });
    auth::Authorization authorization;
    auth::Context guest;
    auth::Context user;
    user.login(auth::Identity{.id = "1"});
    router.get("/guest", [&](http::Request&) -> Task<http::Response> {
        auth::authorize(authorization, guest, "missing");
        co_return http::Response::text("unreachable");
    });
    router.get("/forbidden", [&](http::Request&) -> Task<http::Response> {
        auth::authorize(authorization, user, "missing");
        co_return http::Response::text("unreachable");
    });
    testing::Http http{router};
    assert(http.get("/ok").body() == "ok");
    assert(http.get("/async").body() == "async");
    assert(http.get("/guest").status() == 401);
    assert(http.get("/forbidden").status() == 403);
    http::Request request{http::Method::get, "/"};
    request.set_header("Accept", "application/json");
    const auto denied = http::ExceptionHandler{}.render(request,
        std::make_exception_ptr(auth::AuthorizationError{"denied"}));
    assert(denied.status() == 403 && denied.body().find("denied") != std::string::npos);

    auto a = std::make_shared<view::Engine>("a");
    auto b = std::make_shared<view::Engine>("b");
    auto driver = std::make_shared<database::CallbackDriver>(database::Backend::postgresql,
        [](const String&, const auto&) { return database::Result{}; });
    auto ca = std::make_shared<database::Connection>("a", driver);
    auto cb = std::make_shared<database::Connection>("b", driver);
    const auto ambient = detail::current_execution_context();
    Pause pa;
    HandlePause pb;
    auto ta = scoped(a, ca, "a", pa);
    auto tb = scoped(b, cb, "b", pb);
    auto aa = ta.operator co_await();
    auto ab = tb.operator co_await();
    aa.await_suspend(std::noop_coroutine()).resume();
    assert(detail::current_execution_context() == ambient);
    ab.await_suspend(std::noop_coroutine()).resume();
    assert(detail::current_execution_context() == ambient);
    std::thread worker{[&] {
        const auto before = detail::current_execution_context();
        pa.handle.resume();
        assert(detail::current_execution_context() == before);
    }};
    worker.join();
    const auto first = aa.await_resume();
    assert(first.view == a && first.database == ca && first.trace == "a");
    pb.handle.resume();
    const auto second = ab.await_resume();
    assert(second.view == b && second.database == cb && second.trace == "b");
    assert(detail::current_execution_context() == ambient);
    {
        Pause destroyed;
        auto task = scoped(a, ca, "destroyed", destroyed);
        task.run_inline();
        assert(detail::current_execution_context() == ambient);
    }
    assert(detail::current_execution_context() == ambient);
    Task<int> empty;
    try { (void)empty.operator co_await().await_resume(); assert(false); }
    catch (const std::logic_error&) {}
}
