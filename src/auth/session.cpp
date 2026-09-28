#include <gungnir/auth/session.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <gungnir/auth/context.hpp>
#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>
#include <gungnir/session/session.hpp>

namespace gungnir::auth {

http::MiddlewareHandler
AuthenticateSession::make(
    SessionIdentityResolver resolver,
    SessionOptions options
) {
    if (!resolver) {
        throw std::invalid_argument(
            "Session authentication requires an identity resolver"
        );
    }

    if (options.key.empty()) {
        throw std::invalid_argument(
            "Session authentication key cannot be empty"
        );
    }

    return [
        resolver = std::move(resolver),
        options = std::move(options)
    ](
        http::Request& request,
        http::Next next
    ) -> Task<http::Response> {
        co_return co_await handle(
            request,
            std::move(next),
            resolver,
            options
        );
    };
}

Task<http::Response>
AuthenticateSession::handle(
    http::Request& request,
    http::Next next,
    const SessionIdentityResolver& resolver,
    const SessionOptions& options
) {
    if (!request.has_session()) {
        throw std::logic_error(
            "Session authentication requires session middleware to run first"
        );
    }

    auto& current_session =
        request.session();

    std::optional<Identity> identity;
    bool rotate_session = false;

    if (
        current_session.has(
            options.key
        )
    ) {
        const auto stored_id =
            current_session.get(
                options.key
            );

        if (!stored_id.empty()) {
            identity =
                resolver(
                    stored_id
                );
        }

        if (
            !identity ||
            identity->id.empty() ||
            identity->id != stored_id
        ) {
            identity.reset();

            current_session.forget(
                options.key
            );

            rotate_session = true;
        }
    }

    auto context =
        std::make_shared<Context>(
            std::move(identity)
        );

    request.attach_auth(
        context
    );

    auto response =
        co_await next(request);

    if (context->dirty()) {
        if (
            const auto* user =
                context->user()
        ) {
            current_session.put(
                options.key,
                user->id
            );
        } else {
            current_session.forget(
                options.key
            );
        }

        rotate_session = true;
        context->mark_clean();
    }

    if (rotate_session) {
        current_session.regenerate();
    }

    co_return response;
}

} // namespace gungnir::auth
