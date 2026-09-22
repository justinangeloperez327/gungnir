#include <cassert>
#include <coroutine>
#include <stdexcept>
#include <string>

#include <gungnir/gungnir.hpp>

namespace {

template <typename T>
T sync_wait(gungnir::Task<T> task) {
    task.run_inline();
    auto awaiter = task.operator co_await();
    return awaiter.await_resume();
}

} // namespace

int main() {
    using namespace gungnir;

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
