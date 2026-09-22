#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <gungnir/auth/auth.hpp>

namespace gungnir::auth {

struct Decision {
    bool allowed{false};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return allowed;
    }

    [[nodiscard]] static Decision allow() {
        return {true, {}};
    }

    [[nodiscard]] static Decision deny(std::string message = {}) {
        return {false, std::move(message)};
    }
};

class Authorization {
public:
    using Policy = std::function<Decision(const Identity&)>;

    Authorization& define(std::string ability, Policy policy) {
        policies_.insert_or_assign(std::move(ability), std::move(policy));
        return *this;
    }

    Authorization& before(Policy policy) {
        before_ = std::move(policy);
        return *this;
    }

    [[nodiscard]] Decision inspect(
        std::string_view ability,
        const Identity& identity
    ) const {
        if (before_) {
            const auto decision = (*before_)(identity);
            if (decision.allowed) {
                return decision;
            }
        }

        const auto found = policies_.find(std::string{ability});
        if (found == policies_.end()) {
            return Decision::deny("Authorization ability is not defined");
        }
        return found->second(identity);
    }

    [[nodiscard]] bool allows(
        std::string_view ability,
        const Identity& identity
    ) const {
        return inspect(ability, identity).allowed;
    }

    [[nodiscard]] bool denies(
        std::string_view ability,
        const Identity& identity
    ) const {
        return !allows(ability, identity);
    }

private:
    std::optional<Policy> before_;
    std::unordered_map<std::string, Policy> policies_;
};

} // namespace gungnir::auth
