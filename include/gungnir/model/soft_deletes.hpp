#pragma once

#include <string_view>

namespace gungnir {

class SoftDeletes {
public:
    constexpr explicit SoftDeletes(
        std::string_view column = "deleted_at"
    ) noexcept
        : column_(column) {}

    [[nodiscard]] constexpr std::string_view column() const noexcept {
        return column_;
    }

private:
    std::string_view column_;
};

} // namespace gungnir
