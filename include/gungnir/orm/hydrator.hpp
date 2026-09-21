#pragma once

#include <vector>

#include <gungnir/orm/collection.hpp>

namespace gungnir::orm {

template <typename Model>
class Hydrator {
public:
    using AttributeMap = typename Model::AttributeMap;

    [[nodiscard]] static Model one(const AttributeMap& row) {
        return Model::hydrate(row);
    }

    [[nodiscard]] static Collection<Model> many(
        const std::vector<AttributeMap>& rows
    ) {
        Collection<Model> result;

        for (const auto& row : rows) {
            result.push(Model::hydrate(row));
        }

        return result;
    }
};

} // namespace gungnir::orm
