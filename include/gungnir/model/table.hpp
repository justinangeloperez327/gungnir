#pragma once

#include <string_view>

namespace gungnir {

class Table {
public:
    constexpr explicit Table(std::string_view name) noexcept : name_(name) {}

    [[nodiscard]] constexpr std::string_view name() const noexcept {
        return name_;
    }

private:
    std::string_view name_;
};

} // namespace gungnir
