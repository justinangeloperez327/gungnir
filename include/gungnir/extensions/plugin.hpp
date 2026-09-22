#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <gungnir/core/provider.hpp>
#include <gungnir/extensions/package.hpp>

namespace gungnir::extensions {

class Plugin {
public:
    virtual ~Plugin() = default;
    [[nodiscard]] virtual Package package() const = 0;
    [[nodiscard]] virtual std::shared_ptr<Provider> provider() = 0;
};

class Registry {
public:
    Registry& add(std::shared_ptr<Plugin> plugin) {
        if (!plugin) throw std::invalid_argument("Gungnir plugin cannot be null");
        auto metadata = plugin->package();
        if (!metadata.valid()) throw std::invalid_argument("Gungnir plugin package metadata is incomplete");
        if (!plugins_.emplace(metadata.name, std::move(plugin)).second) {
            throw std::logic_error("Gungnir plugin is already registered: " + metadata.name);
        }
        return *this;
    }

    [[nodiscard]] bool has(std::string_view name) const {
        return plugins_.contains(std::string{name});
    }

    [[nodiscard]] std::shared_ptr<Plugin> get(std::string_view name) const {
        const auto found = plugins_.find(std::string{name});
        if (found == plugins_.end()) throw std::out_of_range("Gungnir plugin is not registered: " + std::string{name});
        return found->second;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Plugin>> plugins_;
};

} // namespace gungnir::extensions
