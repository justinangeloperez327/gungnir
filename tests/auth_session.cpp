#include <cassert>
#include <chrono>
#include <memory>
#include <optional>
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

    session::Options session_options;
    session_options.secure = false;

    std::string current_role{
        "user"
    };

    auth::SessionIdentityResolver resolver =
        [&](
            std::string_view id
        ) -> std::optional<
            auth::Identity
        > {
            if (id != "42") {
                return std::nullopt;
            }

            auth::Identity identity;
            identity.id = "42";
            identity.roles.insert(
                current_role
            );

            return identity;
        };

    routing::Router router;

    router.use(
        session::middleware(
            store,
            session_options
        )
    );

    router.use(
        auth::session(
            resolver
        )
    );

    router.get(
        "/guest",
        [](
            Request& request
        ) {
            assert(
                request.has_auth()
            );

            return Response::text(
                request.guest()
                    ? "guest"
                    : "authenticated"
            );
        }
    );

    router.get(
        "/login",
        [](
            Request& request
        ) -> Task<Response> {
            auth::Identity identity;
            identity.id = "42";
            identity.roles.insert(
                "user"
            );

            request.auth().login(
                std::move(identity)
            );

            co_await sleep_for(
                15ms
            );

            assert(
                request.authenticated()
            );

            assert(
                request.user() !=
                nullptr
            );

            co_return Response::text(
                request.user()->id
            );
        }
    );

    router.get(
        "/me",
        [](
            Request& request
        ) {
            if (request.guest()) {
                return Response::text(
                    "guest"
                );
            }

            const auto* user =
                request.user();

            assert(user != nullptr);

            return Response::text(
                user->id +
                ":" +
                (
                    user->role("admin")
                        ? "admin"
                        : "user"
                )
            );
        }
    );

    router.get(
        "/suspended",
        [](
            Request& request
        ) -> Task<Response> {
            assert(
                request.authenticated()
            );

            const auto before =
                request.user()->id;

            co_await sleep_for(
                15ms
            );

            assert(
                request.authenticated()
            );

            co_return Response::text(
                before +
                ":" +
                request.user()->id
            );
        }
    );

    router.get(
        "/logout",
        [](
            Request& request
        ) {
            request.auth().logout();

            return Response::text(
                request.guest()
                    ? "guest"
                    : "authenticated"
            );
        }
    );

    Request guest{
        http::Method::get,
        "/guest"
    };

    const auto guest_response =
        sync_wait(
            router.dispatch(
                guest
            )
        );

    assert(
        guest_response.body() ==
        "guest"
    );

    const auto anonymous_id =
        session_id(
            guest_response
        );

    assert(
        anonymous_id.size() ==
        64
    );

    auto login =
        request_with_session(
            "/login",
            anonymous_id
        );

    const auto login_response =
        sync_wait(
            router.dispatch(
                login
            )
        );

    assert(
        login_response.body() ==
        "42"
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
        anonymous_id
    );

    assert(
        !store->load(
            anonymous_id
        )
    );

    const auto authenticated_session =
        store->load(
            authenticated_id
        );

    assert(
        authenticated_session
    );

    assert(
        authenticated_session->get(
            "_gungnir_auth_user"
        ) == "42"
    );

    auto me =
        request_with_session(
            "/me",
            authenticated_id
        );

    const auto me_response =
        sync_wait(
            router.dispatch(me)
        );

    assert(
        me_response.body() ==
        "42:user"
    );

    assert(
        me_response.cookies()
            .empty()
    );

    current_role =
        "admin";

    auto refreshed =
        request_with_session(
            "/me",
            authenticated_id
        );

    const auto refreshed_response =
        sync_wait(
            router.dispatch(
                refreshed
            )
        );

    assert(
        refreshed_response.body() ==
        "42:admin"
    );

    auto suspended =
        request_with_session(
            "/suspended",
            authenticated_id
        );

    const auto suspended_response =
        sync_wait(
            router.dispatch(
                suspended
            )
        );

    assert(
        suspended_response.body() ==
        "42:42"
    );

    auto logout =
        request_with_session(
            "/logout",
            authenticated_id
        );

    const auto logout_response =
        sync_wait(
            router.dispatch(
                logout
            )
        );

    assert(
        logout_response.body() ==
        "guest"
    );

    const auto logged_out_id =
        session_id(
            logout_response
        );

    assert(
        logged_out_id.size() ==
        64
    );

    assert(
        logged_out_id !=
        authenticated_id
    );

    assert(
        !store->load(
            authenticated_id
        )
    );

    const auto logged_out_session =
        store->load(
            logged_out_id
        );

    assert(logged_out_session);

    assert(
        !logged_out_session->has(
            "_gungnir_auth_user"
        )
    );

    const auto stale_id =
        security::random_token();

    session::Session stale{
        stale_id
    };

    stale.put(
        "_gungnir_auth_user",
        "missing"
    );

    store->save(stale);

    auto stale_request =
        request_with_session(
            "/me",
            stale_id
        );

    const auto stale_response =
        sync_wait(
            router.dispatch(
                stale_request
            )
        );

    assert(
        stale_response.body() ==
        "guest"
    );

    const auto replacement_id =
        session_id(
            stale_response
        );

    assert(
        replacement_id.size() ==
        64
    );

    assert(
        replacement_id !=
        stale_id
    );

    assert(
        !store->load(stale_id)
    );

    const auto replacement =
        store->load(
            replacement_id
        );

    assert(replacement);

    assert(
        !replacement->has(
            "_gungnir_auth_user"
        )
    );

    return 0;
}
