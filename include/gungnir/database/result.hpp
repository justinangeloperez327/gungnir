#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <gungnir/model/value.hpp>

namespace gungnir::database {

struct Result {
    std::vector<model::AttributeMap> rows;
    std::size_t affected_rows{0};
    std::optional<model::AttributeValue> inserted_id;

    [[nodiscard]] bool empty() const noexcept {
        return rows.empty();
    }
};

} // namespace gungnir::database
