#include <cassert>
#include <coroutine>
#include <memory>
#include <string>

#include <gungnir/gungnir.hpp>

namespace {

template <typename T>
T sync_wait(gungnir::Task<T> task) {
    task.run_inline();
    auto awaiter = task.operator co_await();
    return awaiter.await_resume();
}

class RequestCounter {
public:
    int value{0};
};

class UserController : public gungnir::Controller {
public:
    explicit UserController(gungnir::Container& container)
        : counter_(container.resolve<RequestCounter>()) {}

    gungnir::Response show(gungnir::Request& request) {
        ++counter_->value;

        return text(
            gungnir::String{"user:"} +
            gungnir::String{request.parameter("id")}
        );
    }

    gungnir::Response index() {
        ++counter_->value;
        return text("users");
    }

private:
    std::shared_ptr<RequestCounter> counter_;
};

} // namespace

int main() {
    gungnir::Application app;
    app.singleton<RequestCounter>();

    gungnir::Route::get<UserController>(
        "/users",
        &UserController::index
    );

    gungnir::Route::get<UserController>(
        "/users/{id}",
        &UserController::show
    );

    gungnir::http::Request index_request{
        gungnir::http::Method::get,
        "/users"
    };

    const auto index_response = sync_wait(
        app.router().dispatch(index_request)
    );

    assert(index_response.status() == 200);
    assert(index_response.body() == "users");

    gungnir::http::Request show_request{
        gungnir::http::Method::get,
        "/users/42"
    };

    const auto show_response = sync_wait(
        app.router().dispatch(show_request)
    );

    assert(show_response.status() == 200);
    assert(show_response.body() == "user:42");
    assert(show_request.has_parameter("id"));
    assert(show_request.parameter("id") == "42");

    const auto counter = app.resolve<RequestCounter>();
    assert(counter->value == 2);

    gungnir::http::Request missing_request{
        gungnir::http::Method::get,
        "/missing"
    };

    const auto missing_response = sync_wait(
        app.router().dispatch(missing_request)
    );

    assert(missing_response.status() == 404);
    assert(missing_request.parameters().empty());

    return 0;
}
