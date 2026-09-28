#pragma once

#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gungnir/model/relation.hpp>
#include <gungnir/model/value.hpp>
#include <gungnir/orm/query.hpp>

namespace gungnir::orm {

namespace detail {

template <typename Parent>
[[nodiscard]]
std::optional<model::AttributeValue>
relation_key(
    const Parent& parent,
    const String& key
) {
    return parent.attribute_value(
        key
    );
}

template <typename Related>
[[nodiscard]]
Query<Related> empty_relation_query(
    String column
) {
    return Query<Related>{}
        .where_in(
            std::move(column),
            {}
        );
}

template <typename ModelType>
[[nodiscard]]
String qualified(
    std::string_view column
) {
    return
        String{
            ModelType::table_name()
        } +
        "." +
        String{column};
}

} // namespace detail

template <typename Parent, typename Related>
[[nodiscard]]
Query<Related> relation_query(
    const Parent& parent,
    const HasOne<Related>& relation
) {
    const auto key =
        detail::relation_key(
            parent,
            relation.local_key()
        );

    if (!key) {
        return
            detail::empty_relation_query<
                Related
            >(
                relation.foreign_key()
            );
    }

    return Query<Related>{}
        .where(
            relation.foreign_key(),
            *key
        );
}

template <typename Parent, typename Related>
[[nodiscard]]
Query<Related> relation_query(
    const Parent& parent,
    const HasMany<Related>& relation
) {
    const auto key =
        detail::relation_key(
            parent,
            relation.local_key()
        );

    if (!key) {
        return
            detail::empty_relation_query<
                Related
            >(
                relation.foreign_key()
            );
    }

    return Query<Related>{}
        .where(
            relation.foreign_key(),
            *key
        );
}

template <typename Parent, typename Related>
[[nodiscard]]
Query<Related> relation_query(
    const Parent& parent,
    const BelongsTo<Related>& relation
) {
    const auto key =
        detail::relation_key(
            parent,
            relation.foreign_key()
        );

    if (!key) {
        return
            detail::empty_relation_query<
                Related
            >(
                relation.owner_key()
            );
    }

    return Query<Related>{}
        .where(
            relation.owner_key(),
            *key
        );
}

template <typename Parent, typename Related>
[[nodiscard]]
Query<Related> relation_query(
    const Parent& parent,
    const BelongsToMany<Related>& relation
) {
    const auto related_table =
        String{
            Related::table_name()
        };

    auto query =
        Query<Related>{}
            .select({
                related_table + ".*"
            })
            .join(
                relation.pivot_table(),
                detail::qualified<
                    Related
                >(
                    relation.related_key()
                ),
                Comparison::equal,
                relation.pivot_table() +
                    "." +
                    relation.related_pivot_key()
            );

    const auto key =
        detail::relation_key(
            parent,
            relation.parent_key()
        );

    if (!key) {
        return query.where_in(
            relation.pivot_table() +
                "." +
                relation.foreign_pivot_key(),
            {}
        );
    }

    return query.where(
        relation.pivot_table() +
            "." +
            relation.foreign_pivot_key(),
        *key
    );
}

template <
    typename Parent,
    typename Related,
    typename Through
>
[[nodiscard]]
Query<Related> relation_query(
    const Parent& parent,
    const HasOneThrough<
        Related,
        Through
    >& relation
) {
    const auto related_table =
        String{
            Related::table_name()
        };

    const auto through_table =
        String{
            Through::table_name()
        };

    auto query =
        Query<Related>{}
            .select({
                related_table + ".*"
            })
            .join(
                through_table,
                detail::qualified<
                    Related
                >(
                    relation.second_key()
                ),
                Comparison::equal,
                through_table +
                    "." +
                    relation.second_local_key()
            );

    const auto key =
        detail::relation_key(
            parent,
            relation.local_key()
        );

    if (!key) {
        return query.where_in(
            through_table +
                "." +
                relation.first_key(),
            {}
        );
    }

    return query.where(
        through_table +
            "." +
            relation.first_key(),
        *key
    );
}

template <
    typename Parent,
    typename Related,
    typename Through
>
[[nodiscard]]
Query<Related> relation_query(
    const Parent& parent,
    const HasManyThrough<
        Related,
        Through
    >& relation
) {
    const auto related_table =
        String{
            Related::table_name()
        };

    const auto through_table =
        String{
            Through::table_name()
        };

    auto query =
        Query<Related>{}
            .select({
                related_table + ".*"
            })
            .join(
                through_table,
                detail::qualified<
                    Related
                >(
                    relation.second_key()
                ),
                Comparison::equal,
                through_table +
                    "." +
                    relation.second_local_key()
            );

    const auto key =
        detail::relation_key(
            parent,
            relation.local_key()
        );

    if (!key) {
        return query.where_in(
            through_table +
                "." +
                relation.first_key(),
            {}
        );
    }

    return query.where(
        through_table +
            "." +
            relation.first_key(),
        *key
    );
}

// Relation-only queries cannot be scoped safely because the relation
// metadata does not contain the parent model's key value.
template <typename Related>
Query<Related> relation_query(
    const HasOne<Related>&
) = delete;

template <typename Related>
Query<Related> relation_query(
    const HasMany<Related>&
) = delete;

template <typename Related>
Query<Related> relation_query(
    const BelongsTo<Related>&
) = delete;

template <typename Related>
Query<Related> relation_query(
    const BelongsToMany<Related>&
) = delete;

} // namespace gungnir::orm
