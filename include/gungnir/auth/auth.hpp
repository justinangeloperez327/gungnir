#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace gungnir::auth {

struct Identity {
    std::string id;
    std::unordered_set<std::string> roles;
    std::unordered_map<std::string, std::string> attributes;

    [[nodiscard]] bool role(std::string_view value) const {
        return roles.contains(std::string{value});
    }
};

class Guard {
public:
    using Resolver = std::function<std::optional<Identity>(std::string_view)>;

    explicit Guard(Resolver resolver = {}) : resolver_(std::move(resolver)) {}
    [[nodiscard]] std::optional<Identity> resolve(std::string_view credential) const {
        return resolver_ ? resolver_(credential) : std::nullopt;
    }
private:
    Resolver resolver_;
};

class Gate {
public:
    using Policy = std::function<bool(const Identity&)>;
    Gate& define(std::string ability, Policy policy) {
        policies_.insert_or_assign(std::move(ability), std::move(policy));
        return *this;
    }
    [[nodiscard]] bool allows(std::string_view ability, const Identity& identity) const {
        const auto found = policies_.find(std::string{ability});
        return found != policies_.end() && found->second(identity);
    }
private:
    std::unordered_map<std::string, Policy> policies_;
};

} // namespace gungnir::auth
