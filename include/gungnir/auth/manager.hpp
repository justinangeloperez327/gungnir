#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <gungnir/auth/auth.hpp>

namespace gungnir::auth {

class Manager {
public:
    Manager& guard(std::string name, Guard guard) {
        if (name.empty()) {
            throw std::invalid_argument("Authentication guard name cannot be empty");
        }
        guards_.insert_or_assign(std::move(name), std::move(guard));
        return *this;
    }

    Manager& default_guard(std::string name) {
        default_guard_ = std::move(name);
        return *this;
    }

    [[nodiscard]] const Guard& guard(std::string_view name) const {
        const auto found = guards_.find(std::string{name});
        if (found == guards_.end()) {
            throw std::logic_error("Unknown authentication guard '" + std::string{name} + "'");
        }
        return found->second;
    }

    [[nodiscard]] const Guard& default_guard() const {
        if (default_guard_.empty()) {
            throw std::logic_error("No default authentication guard configured");
        }
        return guard(default_guard_);
    }

    [[nodiscard]] std::optional<Identity> authenticate(
        std::string_view credential,
        std::string_view guard_name = {}
    ) const {
        return guard_name.empty()
            ? default_guard().resolve(credential)
            : guard(guard_name).resolve(credential);
    }

private:
    std::unordered_map<std::string, Guard> guards_;
    std::string default_guard_;
};

} // namespace gungnir::auth
