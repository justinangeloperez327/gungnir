#pragma once

#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gungnir/auth/auth.hpp>
#include <gungnir/http/cookie.hpp>

namespace gungnir::auth {

class SessionGuard;

class Context {
public:
    Context() = default;

    explicit Context(
        std::optional<Identity> identity
    )
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

    void stage_cookie(http::Cookie cookie) {
        if (!active_guard_) throw std::logic_error("Cookie staging requires guard middleware");
        (void)http::serialize_cookie(cookie);
        cookies_.push_back(std::move(cookie));
    }

    void begin_guard(const SessionGuard* owner) {
        if (!owner || active_guard_) throw std::logic_error("Guard middleware must be installed once per request");
        active_guard_ = owner;
    }

    void end_guard() noexcept {
        cookies_.clear();
        active_guard_ = nullptr;
    }

    [[nodiscard]] const SessionGuard* active_guard() const noexcept { return active_guard_; }

    [[nodiscard]] std::vector<http::Cookie> take_cookies() {
        return std::exchange(cookies_, {});
    }

private:
    std::optional<Identity> identity_;
    bool dirty_{false};
    std::vector<http::Cookie> cookies_;
    const SessionGuard* active_guard_{nullptr};
};

} // namespace gungnir::auth
