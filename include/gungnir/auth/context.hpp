#pragma once

#include <optional>
#include <stdexcept>
#include <utility>

#include <gungnir/auth/auth.hpp>

namespace gungnir::auth {

class Context {
public:
    Context() = default;

    explicit Context(
        std::optional<Identity> identity
    ) noexcept
        : identity_(
            std::move(identity)
          ) {}

    void login(
        Identity identity
    ) {
        if (identity.id.empty()) {
            throw std::invalid_argument(
                "Authenticated identity requires a non-empty id"
            );
        }

        identity_ =
            std::move(identity);

        dirty_ = true;
    }

    void logout() noexcept {
        if (!identity_) {
            return;
        }

        identity_.reset();
        dirty_ = true;
    }

    [[nodiscard]]
    bool check()
        const noexcept {
        return identity_.has_value();
    }

    [[nodiscard]]
    bool guest()
        const noexcept {
        return !check();
    }

    [[nodiscard]]
    const Identity* user()
        const noexcept {
        return
            identity_
                ? &*identity_
                : nullptr;
    }

    [[nodiscard]]
    const std::optional<Identity>&
    identity()
        const noexcept {
        return identity_;
    }

    [[nodiscard]]
    bool dirty()
        const noexcept {
        return dirty_;
    }

    void mark_clean()
        noexcept {
        dirty_ = false;
    }

private:
    std::optional<Identity> identity_;
    bool dirty_{false};
};

} // namespace gungnir::auth
