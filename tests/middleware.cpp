#include <cassert>
#include <chrono>
#include <coroutine>
#include <memory>
#include <string>
#include <vector>

#include <gungnir/gungnir.hpp>

namespace {

template <typename T>
T sync_wait(gungnir::Task<T> task) {
    task.run_inline();
    auto awaiter = task.operator co_await();
    return awaiter.await_resume();
}

class Trace {
public:
    std::vector<std::string> entries;
};

class GlobalMiddleware :
    public gungnir::Middleware {
public:
    explicit GlobalMiddleware(
        gungnir::Container& container
    )
        : trace_(
            container.resolve<Trace>()
        ) {}

    gungnir::Task<gungnir::Response> handle(
        gungnir::Request& request,
        gungnir::Next next
    ) {
        trace_->entries.push_back(
            "global:before"
        );

        auto response =
            co_await next(request);

        trace_->entries.push_back(
            "global:after"
        );

        co_return response;
    }

private:
    std::shared_ptr<Trace> trace_;
};

class AuthMiddleware :
    public gungnir::Middleware {
public:
    gungnir::Task<gungnir::Response> handle(
        gungnir::Request& request,
        gungnir::Next next
    ) {
        if (
            request.header("authorization") !=
            "Bearer ok"
        ) {
            co_return gungnir::Response::text(
                "Unauthorized",
                401
            );
        }

        co_return co_await next(request);
    }
};

class UserController :
    public gungnir::Controller {
public:
    gungnir::Response index(
        gungnir::Request& request
    ) {
        return text(
            request.input("name")
        );
    }
};

} // namespace

int main() {
    gungnir::Application app;

    app.singleton<Trace>();
    app.middleware<GlobalMiddleware>();

    gungnir::Route::get<UserController>(
        "/users",
        &UserController::index
    ).middleware<AuthMiddleware>();

    gungnir::Request denied{
        gungnir::http::Method::get,
        "/users?name=Justin"
    };

    const auto denied_response =
        sync_wait(
            app.router().dispatch(denied)
        );

    assert(denied_response.status() == 401);

    gungnir::Request allowed{
        gungnir::http::Method::get,
        "/users?name=Justin"
    };

    allowed.set_header(
        "Authorization",
        "Bearer ok"
    );

    const auto allowed_response =
        sync_wait(
            app.router().dispatch(allowed)
        );

    assert(allowed_response.status() == 200);
    assert(
        allowed_response.body() ==
        "Justin"
    );

    const auto trace =
        app.resolve<Trace>();

    assert(trace->entries.size() == 4);
    assert(
        trace->entries[0] ==
        "global:before"
    );
    assert(
        trace->entries[1] ==
        "global:after"
    );
    assert(
        trace->entries[2] ==
        "global:before"
    );
    assert(
        trace->entries[3] ==
        "global:after"
    );

    auto terminal =
        [](gungnir::Request&) -> gungnir::Task<gungnir::Response> {
            co_return gungnir::Response::text(
                "ok"
            );
        };

    auto limiter =
        gungnir::http::rate_limit(
            gungnir::http::RateLimitOptions{
                .requests = 1,
                .window = std::chrono::seconds{60}
            }
        );

    gungnir::Request first_limited{
        gungnir::http::Method::get,
        "/limited"
    };

    first_limited.client_ip(
        "203.0.113.10"
    );
    first_limited.set_header(
        "X-Forwarded-For",
        "198.51.100.1"
    );

    assert(
        sync_wait(
            limiter(first_limited, terminal)
        ).status() == 200
    );

    gungnir::Request spoofed_same_client{
        gungnir::http::Method::get,
        "/limited"
    };

    spoofed_same_client.client_ip(
        "203.0.113.10"
    );
    spoofed_same_client.set_header(
        "X-Forwarded-For",
        "198.51.100.2"
    );

    assert(
        sync_wait(
            limiter(spoofed_same_client, terminal)
        ).status() == 429
    );

    auto trust_proxy =
        gungnir::http::trusted_proxies(
            gungnir::http::TrustedProxyOptions{
                .proxies = {
                    "127.0.0.1"
                }
            }
        );

    gungnir::Request proxied{
        gungnir::http::Method::get,
        "/proxied"
    };

    proxied.client_ip(
        "127.0.0.1"
    );
    proxied.set_header(
        "X-Forwarded-For",
        "198.51.100.77, 127.0.0.1"
    );

    assert(
        sync_wait(
            trust_proxy(proxied, terminal)
        ).status() == 200
    );
    assert(
        proxied.client_ip() ==
        "198.51.100.77"
    );

    gungnir::Request untrusted_proxy{
        gungnir::http::Method::get,
        "/proxied"
    };

    untrusted_proxy.client_ip(
        "203.0.113.50"
    );
    untrusted_proxy.set_header(
        "X-Forwarded-For",
        "198.51.100.88"
    );

    assert(
        sync_wait(
            trust_proxy(
                untrusted_proxy,
                terminal
            )
        ).status() == 200
    );
    assert(
        untrusted_proxy.client_ip() ==
        "203.0.113.50"
    );

    return 0;
}
