#pragma once

#include <gungnir/model/relation.hpp>
#include <gungnir/orm/query.hpp>

namespace gungnir::orm {

template <typename Related>
[[nodiscard]] Query<Related> relation_query(const HasOne<Related>&) {
    return Query<Related>{};
}

template <typename Related>
[[nodiscard]] Query<Related> relation_query(const HasMany<Related>&) {
    return Query<Related>{};
}

template <typename Related>
[[nodiscard]] Query<Related> relation_query(const BelongsTo<Related>&) {
    return Query<Related>{};
}

template <typename Related>
[[nodiscard]] Query<Related> relation_query(const BelongsToMany<Related>&) {
    return Query<Related>{};
}

} // namespace gungnir::orm
