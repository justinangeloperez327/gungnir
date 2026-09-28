#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <gungnir/gungnir.hpp>

namespace {

using namespace std::chrono_literals;

template <typename T>
T sync_wait(
    gungnir::Task<T> task
) {
    task.run_inline();

    for (
        int attempt = 0;
        attempt < 400 &&
            !task.done();
        ++attempt
    ) {
        std::this_thread::sleep_for(
            5ms
        );
    }

    assert(task.done());

    auto awaiter =
        task.operator co_await();

    return awaiter.await_resume();
}

std::string session_id(
    const gungnir::Response& response
) {
    for (
        const auto& cookie :
        response.cookies()
    ) {
        if (
            cookie.name ==
            "gungnir_session"
        ) {
            return cookie.value;
        }
    }

    return {};
}

gungnir::Request request_with_session(
    std::string path,
    std::string id
) {
    gungnir::Request request{
        gungnir::http::Method::get,
        std::move(path)
    };

    request.set_header(
        "Cookie",
        "gungnir_session=" +
            std::move(id)
    );

    return request;
}

} // namespace

int main() {
    using namespace gungnir;

    auto store =
        std::make_shared<
            session::MemoryStore
        >();

    session::Options options;
    options.secure = false;

    routing::Router router;

    router.use(
        session::middleware(
            store,
            options
        )
    );

    router.get(
        "/create",
        [](
            Request& request
        ) -> Task<Response> {
            assert(
                request.has_session()
            );

            request.session().put(
                "user_id",
                "42"
            );

            request.session().flash(
                "status",
                "saved"
            );

            co_await sleep_for(
                15ms
            );

            assert(
                request.session()
                    .get("user_id") ==
                "42"
            );

            co_return Response::text(
                std::string{
                    request.session().id()
                }
            );
        }
    );

    router.get(
        "/read",
        [](
            Request& request
        ) {
            return Response::text(
                std::string{
                    request.session()
                        .get("user_id")
                } +
                ":" +
                std::string{
                    request.session()
                        .flashed("status")
                }
            );
        }
    );

    router.get(
        "/regenerate",
        [](
            Request& request
        ) {
            request.session()
                .regenerate();

            return Response::text(
                "rotated"
            );
        }
    );

    router.get(
        "/invalidate",
        [](
            Request& request
        ) {
            request.session()
                .invalidate();

            return Response::text(
                request.session()
                    .has("user_id")
                    ? "retained"
                    : "cleared"
            );
        }
    );

    Request create{
        http::Method::get,
        "/create"
    };

    const auto created =
        sync_wait(
            router.dispatch(create)
        );

    const auto first_id =
        session_id(created);

    assert(
        first_id.size() == 64
    );

    assert(
        created.body() ==
        first_id
    );

    assert(
        created.cookies().size() ==
        1
    );

    assert(
        !created.cookies()
            .front()
            .secure
    );

    assert(
        created.cookies()
            .front()
            .http_only
    );

    const auto first_saved =
        store->load(first_id);

    assert(first_saved);
    assert(
        first_saved->get(
            "user_id"
        ) == "42"
    );

    auto read =
        request_with_session(
            "/read",
            first_id
        );

    const auto read_response =
        sync_wait(
            router.dispatch(read)
        );

    assert(
        read_response.body() ==
        "42:saved"
    );

    assert(
        read_response.cookies()
            .empty()
    );

    auto read_again =
        request_with_session(
            "/read",
            first_id
        );

    const auto read_again_response =
        sync_wait(
            router.dispatch(
                read_again
            )
        );

    assert(
        read_again_response.body() ==
        "42:"
    );

    auto rotate =
        request_with_session(
            "/regenerate",
            first_id
        );

    const auto rotated =
        sync_wait(
            router.dispatch(rotate)
        );

    const auto second_id =
        session_id(rotated);

    assert(
        second_id.size() == 64
    );

    assert(
        second_id != first_id
    );

    assert(
        !store->load(first_id)
    );

    const auto second_saved =
        store->load(second_id);

    assert(second_saved);
    assert(
        second_saved->get(
            "user_id"
        ) == "42"
    );

    auto invalidate =
        request_with_session(
            "/invalidate",
            second_id
        );

    const auto invalidated =
        sync_wait(
            router.dispatch(
                invalidate
            )
        );

    const auto third_id =
        session_id(
            invalidated
        );

    assert(
        invalidated.body() ==
        "cleared"
    );

    assert(
        third_id.size() == 64
    );

    assert(
        third_id != second_id
    );

    assert(
        !store->load(second_id)
    );

    const auto third_saved =
        store->load(third_id);

    assert(third_saved);
    assert(
        !third_saved->has(
            "user_id"
        )
    );

    session::Session weak{
        "weak"
    };

    weak.put(
        "user_id",
        "999"
    );

    store->save(weak);

    auto attacker =
        request_with_session(
            "/read",
            "weak"
        );

    const auto rejected =
        sync_wait(
            router.dispatch(
                attacker
            )
        );

    assert(
        rejected.body() ==
        ":"
    );

    const auto safe_id =
        session_id(rejected);

    assert(
        safe_id.size() == 64
    );

    assert(
        safe_id != "weak"
    );

    return 0;
}
