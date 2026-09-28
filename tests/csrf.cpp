#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include <gungnir/gungnir.hpp>

namespace {

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
            std::chrono::milliseconds{5}
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
    gungnir::http::Method method,
    std::string path,
    const std::string& id
) {
    gungnir::Request request{
        method,
        std::move(path)
    };

    request.set_header(
        "Cookie",
        "gungnir_session=" + id
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

    session::Options session_options;
    session_options.secure = false;

    routing::Router router;

    router.use(
        session::middleware(
            store,
            session_options
        )
    );

    router.use(
        http::csrf()
    );

    router.use(
        auth::session(
            [](
                std::string_view id
            ) -> std::optional<
                auth::Identity
            > {
                if (id != "42") {
                    return std::nullopt;
                }

                auth::Identity identity;
                identity.id = "42";

                return identity;
            }
        )
    );

    int submitted = 0;

    router.get(
        "/form",
        [](
            Request& request
        ) {
            return Response::text(
                std::string{
                    http::csrf_token(
                        request
                    )
                }
            );
        }
    );

    router.post(
        "/submit",
        [&](
            Request&
        ) {
            ++submitted;

            return Response::text(
                "saved"
            );
        }
    );

    router.post(
        "/login",
        [](
            Request& request
        ) {
            auth::Identity identity;
            identity.id = "42";

            request.auth().login(
                std::move(identity)
            );

            return Response::text(
                "logged-in"
            );
        }
    );

    Request form{
        http::Method::get,
        "/form"
    };

    const auto form_response =
        sync_wait(
            router.dispatch(form)
        );

    assert(
        form_response.status() ==
        200
    );

    const auto first_id =
        session_id(
            form_response
        );

    const auto first_token =
        std::string{
            form_response.body()
        };

    assert(
        first_id.size() ==
        64
    );

    assert(
        first_token.size() ==
        64
    );

    auto missing =
        request_with_session(
            http::Method::post,
            "/submit",
            first_id
        );

    const auto missing_response =
        sync_wait(
            router.dispatch(
                missing
            )
        );

    assert(
        missing_response.status() ==
        419
    );

    assert(submitted == 0);

    auto query_only =
        request_with_session(
            http::Method::post,
            "/submit?_token=" +
                first_token,
            first_id
        );

    const auto query_response =
        sync_wait(
            router.dispatch(
                query_only
            )
        );

    assert(
        query_response.status() ==
        419
    );

    assert(submitted == 0);

    auto header =
        request_with_session(
            http::Method::post,
            "/submit",
            first_id
        );

    header.set_header(
        "X-CSRF-Token",
        first_token
    );

    const auto header_response =
        sync_wait(
            router.dispatch(
                header
            )
        );

    assert(
        header_response.status() ==
        200
    );

    assert(
        header_response.body() ==
        "saved"
    );

    assert(submitted == 1);

    Request form_post{
        http::Method::post,
        "/submit",
        "_token=" + first_token
    };

    form_post.set_header(
        "Cookie",
        "gungnir_session=" +
            first_id
    );

    form_post.set_header(
        "Content-Type",
        "application/x-www-form-urlencoded"
    );

    const auto form_post_response =
        sync_wait(
            router.dispatch(
                form_post
            )
        );

    assert(
        form_post_response.status() ==
        200
    );

    assert(submitted == 2);

    auto login =
        request_with_session(
            http::Method::post,
            "/login",
            first_id
        );

    login.set_header(
        "X-CSRF-Token",
        first_token
    );

    const auto login_response =
        sync_wait(
            router.dispatch(
                login
            )
        );

    assert(
        login_response.status() ==
        200
    );

    const auto authenticated_id =
        session_id(
            login_response
        );

    assert(
        authenticated_id.size() ==
        64
    );

    assert(
        authenticated_id !=
        first_id
    );

    assert(
        !store->load(first_id)
    );

    const auto authenticated =
        store->load(
            authenticated_id
        );

    assert(authenticated);

    const auto rotated_token =
        std::string{
            authenticated->get(
                "_gungnir_csrf_token"
            )
        };

    assert(
        rotated_token.size() ==
        64
    );

    assert(
        rotated_token !=
        first_token
    );

    auto stale_token =
        request_with_session(
            http::Method::post,
            "/submit",
            authenticated_id
        );

    stale_token.set_header(
        "X-CSRF-Token",
        first_token
    );

    const auto stale_response =
        sync_wait(
            router.dispatch(
                stale_token
            )
        );

    assert(
        stale_response.status() ==
        419
    );

    assert(submitted == 2);

    auto current_token =
        request_with_session(
            http::Method::post,
            "/submit",
            authenticated_id
        );

    current_token.set_header(
        "X-CSRF-Token",
        rotated_token
    );

    const auto current_response =
        sync_wait(
            router.dispatch(
                current_token
            )
        );

    assert(
        current_response.status() ==
        200
    );

    assert(submitted == 3);

    return 0;
}
