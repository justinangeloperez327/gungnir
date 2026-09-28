#pragma once

#include <filesystem>
#include <shared_mutex>
#include <string_view>

#include <gungnir/core/types.hpp>
#include <gungnir/view/data.hpp>

namespace gungnir::view {

class Engine {
public:
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
};

} // namespace gungnir::view
