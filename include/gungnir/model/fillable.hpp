#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace gungnir {

template <std::size_t Count>
class Fillable {
public:
    template <typename... Names>
    requires (sizeof...(Names) == Count)
    constexpr explicit Fillable(Names... names) noexcept
        : names_{std::string_view{names}...} {}

    [[nodiscard]] constexpr bool contains(std::string_view name) const noexcept {
        return std::find(names_.begin(), names_.end(), name) != names_.end();
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept {
        return names_.size();
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return names_.empty();
    }

    [[nodiscard]] constexpr std::span<const std::string_view> values() const noexcept {
        return names_;
    }

private:
    std::array<std::string_view, Count> names_;
};

template <typename... Names>
Fillable(Names...) -> Fillable<sizeof...(Names)>;

} // namespace gungnir
