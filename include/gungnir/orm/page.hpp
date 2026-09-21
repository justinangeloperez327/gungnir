#pragma once

#include <algorithm>
#include <cstddef>

#include <gungnir/orm/collection.hpp>

namespace gungnir::orm {

template <typename Model>
struct Page {
    Collection<Model> data;
    std::size_t current_page{1};
    std::size_t per_page{15};
    std::size_t total{0};
    std::size_t last_page{1};

    [[nodiscard]] bool empty() const noexcept {
        return data.empty();
    }

    [[nodiscard]] bool has_more() const noexcept {
        return current_page < last_page;
    }

    [[nodiscard]] bool has_previous() const noexcept {
        return current_page > 1;
    }
};

} // namespace gungnir::orm
