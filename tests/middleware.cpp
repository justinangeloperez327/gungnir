#include <cassert>
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

    return 0;
}
