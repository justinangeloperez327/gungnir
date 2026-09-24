#include <atomic>
#include <cassert>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <gungnir/gungnir.hpp>

namespace {

using namespace std::chrono_literals;

class RequestState {
public:
    RequestState()
        : id(
            next.fetch_add(1) + 1
          ) {}

    int id;

private:
    inline static std::atomic_int next{0};
};

class ScopedController :
    public gungnir::Controller {
public:
    explicit ScopedController(
        gungnir::ServiceScope& services
    )
        : state_(
            services.resolve<
                RequestState
            >()
          ) {}

    gungnir::Task<gungnir::Response>
    show(
        gungnir::Request& request
    ) {
        const auto before =
            state_;

        co_await gungnir::sleep_for(
            25ms
        );

        const auto after =
            request.services()
                .resolve<
                    RequestState
                >();

        co_return text(
            std::to_string(
                before->id
            ) +
            (
                before == after
                ? ":same"
                : ":different"
            )
        );
    }

private:
    std::shared_ptr<RequestState>
        state_;
};

template <typename T>
T wait(
    gungnir::Task<T>& task
) {
    for (
        int attempt = 0;
        attempt < 5000 &&
            !task.done();
        ++attempt
    ) {
        std::this_thread::sleep_for(
            1ms
        );
    }

    if (!task.done()) {
        throw std::runtime_error(
            "Scoped request task did not complete"
        );
    }

    auto awaiter =
        task.operator co_await();

    return awaiter.await_resume();
}

} // namespace

int main() {
    using namespace gungnir;

    Application app;

    app.scoped<RequestState>();

    auto first_scope =
        app.container().scope();

    const auto first =
        first_scope.resolve<
            RequestState
        >();

    const auto first_again =
        first_scope.resolve<
            RequestState
        >();

    assert(
        first == first_again
    );

    auto second_scope =
        app.container().scope();

    const auto second =
        second_scope.resolve<
            RequestState
        >();

    assert(
        first != second
    );

    bool root_rejected = false;

    try {
        static_cast<void>(
            app.resolve<
                RequestState
            >()
        );
    } catch (
        const std::logic_error&
    ) {
        root_rejected = true;
    }

    assert(root_rejected);

    Route::get<
        ScopedController
    >(
        "/scope",
        &ScopedController::show
    );

    http::Request first_request{
        http::Method::get,
        "/scope"
    };

    http::Request second_request{
        http::Method::get,
        "/scope"
    };

    auto first_task =
        app.router().dispatch(
            first_request
        );

    auto second_task =
        app.router().dispatch(
            second_request
        );

    first_task.run_inline();
    second_task.run_inline();

    const auto first_response =
        wait(first_task);

    const auto second_response =
        wait(second_task);

    assert(
        first_response.status() ==
        200
    );

    assert(
        second_response.status() ==
        200
    );

    assert(
        first_response.body()
            .ends_with(":same")
    );

    assert(
        second_response.body()
            .ends_with(":same")
    );

    assert(
        first_response.body() !=
        second_response.body()
    );

    assert(
        first_request.has_services()
    );

    assert(
        second_request.has_services()
    );

    const auto first_request_state =
        first_request.services()
            .resolve<RequestState>();

    const auto second_request_state =
        second_request.services()
            .resolve<RequestState>();

    assert(
        first_request_state !=
        second_request_state
    );

    return 0;
}
