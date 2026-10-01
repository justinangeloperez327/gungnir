#pragma once

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <string_view>

#include <gungnir/core/types.hpp>
#include <gungnir/view/data.hpp>

namespace gungnir::view {

class Engine {
public:
    using Helper = std::function<Value(const std::vector<Value>&)>;
    Engine& helper(String name, Helper helper);
    [[nodiscard]] String source(std::string_view name) const;
    [[nodiscard]] Value call(std::string_view name, const std::vector<Value>& arguments) const;
    explicit Engine(std::filesystem::path root = "views");

    Engine& root(std::filesystem::path value);
    [[nodiscard]]
    std::filesystem::path root()
        const;

    [[nodiscard]] String render(
        std::string_view name,
        const Data& data = {}
    ) const;

    [[nodiscard]] String render_text(
        std::string_view source,
        const Data& data = {}
    ) const;

private:
    mutable std::shared_mutex mutex_;
    std::filesystem::path root_;
    std::unordered_map<String, Helper> helpers_;
};

} // namespace gungnir::view
