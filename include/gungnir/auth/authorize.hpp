#pragma once

#include <stdexcept>
#include <string>
#include <utility>

#include <gungnir/auth/authorization.hpp>
#include <gungnir/auth/context.hpp>

namespace gungnir::auth {

class AuthorizationError : public std::runtime_error {
public:
    explicit AuthorizationError(std::string message)
        : std::runtime_error(std::move(message)) {}
};

inline void authorize(
    const Authorization& authorization,
    const Context& context,
    std::string_view ability
) {
    const auto* identity = context.user();
    if (identity == nullptr) {
        throw AuthorizationError{"Authentication required"};
    }

    const auto decision = authorization.inspect(ability, *identity);
    if (!decision.allowed) {
        throw AuthorizationError{
            decision.message.empty() ? "Action is not authorized" : decision.message
        };
    }
}

} // namespace gungnir::auth
