#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <gungnir/http/middleware.hpp>

namespace gungnir::http {

class MiddlewareRegistry {
public:
    MiddlewareRegistry& alias(
        std::string name,
        MiddlewareHandler handler
    ) {
        if (name.empty() || !handler) {
            throw std::invalid_argument(
                "Middleware alias requires a name and handler"
            );
        }

        aliases_.insert_or_assign(
            std::move(name),
            std::move(handler)
        );

        return *this;
    }

    MiddlewareRegistry& group(
        std::string name,
        std::vector<std::string> aliases
    ) {
        if (name.empty()) {
            throw std::invalid_argument(
                "Middleware group requires a name"
            );
        }

        groups_.insert_or_assign(
            std::move(name),
            std::move(aliases)
        );

        return *this;
    }

    MiddlewareRegistry& priority(
        std::vector<std::string> aliases
    ) {
        priority_.clear();

        for (std::size_t index = 0; index < aliases.size(); ++index) {
            priority_.insert_or_assign(
                std::move(aliases[index]),
                index
            );
        }

        return *this;
    }

    [[nodiscard]] MiddlewareHandler resolve(
        std::string_view name
    ) const {
        const auto found = aliases_.find(std::string{name});

        if (found == aliases_.end()) {
            throw std::out_of_range(
                "Unknown middleware alias: " + std::string{name}
            );
        }

        return found->second;
    }

    [[nodiscard]] std::vector<MiddlewareHandler> resolve_group(
        std::string_view name
    ) const {
        const auto found = groups_.find(std::string{name});

        if (found == groups_.end()) {
            throw std::out_of_range(
                "Unknown middleware group: " + std::string{name}
            );
        }

        auto names = found->second;

        std::stable_sort(
            names.begin(),
            names.end(),
            [this](const auto& left, const auto& right) {
                return rank(left) < rank(right);
            }
        );

        std::vector<MiddlewareHandler> result;
        result.reserve(names.size());

        for (const auto& alias_name : names) {
            result.push_back(resolve(alias_name));
        }

        return result;
    }

private:
    [[nodiscard]] std::size_t rank(
        const std::string& name
    ) const noexcept {
        const auto found = priority_.find(name);

        return found == priority_.end()
            ? priority_.size()
            : found->second;
    }

    std::unordered_map<std::string, MiddlewareHandler> aliases_;
    std::unordered_map<std::string, std::vector<std::string>> groups_;
    std::unordered_map<std::string, std::size_t> priority_;
};

} // namespace gungnir::http
