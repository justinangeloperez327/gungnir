#pragma once

#include <filesystem>
#include <string_view>

#include <gungnir/core/types.hpp>
#include <gungnir/view/data.hpp>

namespace gungnir::view {

class Engine {
public:
    explicit Engine(std::filesystem::path root = "views");

    Engine& root(std::filesystem::path value);
    [[nodiscard]] const std::filesystem::path& root() const noexcept;

    [[nodiscard]] String render(
        std::string_view name,
        const Data& data = {}
    ) const;

    [[nodiscard]] String render_text(
        std::string_view source,
        const Data& data = {}
    ) const;

private:
    std::filesystem::path root_;
};

} // namespace gungnir::view
