#pragma once
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>

namespace gungnir::routing {

class BindingRegistry {
public:
    using Resolver = std::function<std::optional<std::string>(std::string_view)>;

    template <typename Model>
    void bind(Resolver resolver) {
        bindings_.insert_or_assign(std::type_index{typeid(Model)}, std::move(resolver));
    }

    template <typename Model>
    [[nodiscard]] std::optional<std::string> resolve(std::string_view value) const {
        const auto found = bindings_.find(std::type_index{typeid(Model)});
        return found == bindings_.end() ? std::nullopt : found->second(value);
    }

private:
    std::unordered_map<std::type_index, Resolver> bindings_;
};

} // namespace gungnir::routing
