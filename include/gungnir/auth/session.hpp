#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <gungnir/auth/auth.hpp>
#include <gungnir/http/middleware.hpp>

namespace gungnir::auth {

using SessionIdentityResolver =
    std::function<
        std::optional<Identity>(
            std::string_view
        )
    >;

struct SessionOptions {
    std::string key{
        "_gungnir_auth_user"
    };
};

class AuthenticateSession {
public:
    [[nodiscard]]
    static http::MiddlewareHandler make(
        SessionIdentityResolver resolver,
        SessionOptions options = {}
    );

private:
    static Task<http::Response> handle(
        http::Request& request,
        http::Next next,
        const SessionIdentityResolver& resolver,
        const SessionOptions& options
    );
};

[[nodiscard]]
inline http::MiddlewareHandler session(
    SessionIdentityResolver resolver,
    SessionOptions options = {}
) {
    return AuthenticateSession::make(
        std::move(resolver),
        std::move(options)
    );
}

} // namespace gungnir::auth
