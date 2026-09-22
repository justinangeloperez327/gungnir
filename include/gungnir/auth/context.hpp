#pragma once

#include <optional>
#include <utility>

#include <gungnir/auth/auth.hpp>

namespace gungnir::auth {

class Context {
public:
    void login(Identity identity) {
        identity_ = std::move(identity);
    }

    void logout() noexcept {
        identity_.reset();
    }

    [[nodiscard]] bool check() const noexcept {
        return identity_.has_value();
    }

    [[nodiscard]] bool guest() const noexcept {
        return !check();
    }

    [[nodiscard]] const Identity* user() const noexcept {
        return identity_ ? &*identity_ : nullptr;
    }

    [[nodiscard]] const std::optional<Identity>& identity() const noexcept {
        return identity_;
    }

private:
    std::optional<Identity> identity_;
};

} // namespace gungnir::auth
