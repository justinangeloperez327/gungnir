#pragma once

#include <type_traits>
#include <utility>

#include <gungnir/model/relation.hpp>
#include <gungnir/orm/query.hpp>

namespace gungnir::orm {

template <typename Related>
[[nodiscard]] Query<Related> related_query(const HasOne<Related>& relation) {
    return Query<Related>{}.where(
        relation.foreign_key(),
        model::AttributeValue{}
    );
}

template <typename Related>
[[nodiscard]] Query<Related> related_query(const HasMany<Related>& relation) {
    return Query<Related>{}.where(
        relation.foreign_key(),
        model::AttributeValue{}
    );
}

template <typename Related>
[[nodiscard]] Query<Related> related_query(const BelongsTo<Related>& relation) {
    return Query<Related>{}.where(
        relation.owner_key(),
        model::AttributeValue{}
    );
}

} // namespace gungnir::orm
