#include <gungnir/session/middleware.hpp>

#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>
#include <gungnir/security/security.hpp>
#include <gungnir/session/session.hpp>

namespace gungnir::session {

namespace {

constexpr std::size_t session_id_bytes =
    32;

constexpr std::size_t session_id_length =
    session_id_bytes * 2;

bool valid_session_id(
    std::string_view value
) noexcept {
    if (
        value.size() !=
        session_id_length
    ) {
        return false;
    }

    for (const auto character : value) {
        if (
            !std::isxdigit(
                static_cast<unsigned char>(
                    character
                )
            )
        ) {
            return false;
        }
    }

    return true;
}

std::shared_ptr<Session>
load_or_create(
    const http::Request& request,
    const std::shared_ptr<Store>& store,
    const Options& options,
    std::string& persisted_id,
    bool& created
) {
    const auto cookie =
        request.cookie(
            options.cookie_name
        );

    if (valid_session_id(cookie)) {
        if (
            auto loaded =
                store->load(cookie)
        ) {
            persisted_id =
                std::string{cookie};

            created = false;

            return std::make_shared<
                Session
            >(
                std::move(*loaded)
            );
        }
    }

    created = true;

    return std::make_shared<
        Session
    >(
        security::random_token(
            session_id_bytes
        )
    );
}

http::Cookie session_cookie(
    const Session& session,
    const Options& options
) {
    return http::Cookie{
        .name =
            options.cookie_name,
        .value =
            std::string{
                session.id()
            },
        .path =
            options.path,
        .domain =
            options.domain,
        .max_age =
            std::nullopt,
        .secure =
            options.secure,
        .http_only =
            true,
        .same_site =
            options.same_site
    };
}

} // namespace

http::MiddlewareHandler
StartSession::make(
    std::shared_ptr<Store> store,
    Options options
) {
    if (!store) {
        throw std::invalid_argument(
            "Session middleware requires a store"
        );
    }

    if (
        !security::valid_cookie_name(
            options.cookie_name
        )
    ) {
        throw std::invalid_argument(
            "Session cookie name is invalid"
        );
    }

    if (
        options.path.empty() ||
        options.path.front() != '/'
    ) {
        throw std::invalid_argument(
            "Session cookie path must be absolute"
        );
    }

    return [
        store = std::move(store),
        options = std::move(options)
    ](
        http::Request& request,
        http::Next next
    ) -> Task<http::Response> {
        co_return co_await handle(
            request,
            std::move(next),
            store,
            options
        );
    };
}

Task<http::Response>
StartSession::handle(
    http::Request& request,
    http::Next next,
    const std::shared_ptr<Store>& store,
    const Options& options
) {
    std::string persisted_id;
    bool created = false;

    auto current =
        load_or_create(
            request,
            store,
            options,
            persisted_id,
            created
        );

    current->age_flash();

    request.attach_session(
        current
    );

    auto response =
        co_await next(request);

    if (
        current->id().empty() ||
        !valid_session_id(
            current->id()
        )
    ) {
        current->regenerate();
    }

    const auto current_id =
        std::string{
            current->id()
        };

    const bool rotated =
        current->regenerated() ||
        (
            !persisted_id.empty() &&
            persisted_id != current_id
        );

    if (
        rotated &&
        !persisted_id.empty() &&
        persisted_id != current_id
    ) {
        store->erase(
            persisted_id
        );
    }

    store->save(
        *current
    );

    current->mark_persisted();

    if (
        created ||
        rotated ||
        persisted_id.empty()
    ) {
        response.cookie(
            session_cookie(
                *current,
                options
            )
        );
    }

    co_return response;
}

} // namespace gungnir::session
