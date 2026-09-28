#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <gungnir/database/connection.hpp>
#include <gungnir/database/runtime.hpp>
#include <gungnir/database/transaction.hpp>
#include <gungnir/model/relation.hpp>
#include <gungnir/model/value.hpp>
#include <gungnir/orm/advanced_mutation.hpp>
#include <gungnir/orm/compiler.hpp>
#include <gungnir/orm/error.hpp>
#include <gungnir/orm/plan.hpp>
#include <gungnir/orm/query_log.hpp>

namespace gungnir::orm {

struct PivotSyncResult {
    std::vector<
        model::AttributeValue
    > attached;

    std::vector<
        model::AttributeValue
    > detached;

    [[nodiscard]]
    bool changed()
        const noexcept {
        return
            !attached.empty() ||
            !detached.empty();
    }
};

namespace detail {

inline void ensure_pivot_backend(
    database::Backend backend
) {
    if (
        backend ==
        database::Backend::mongodb
    ) {
        throw QueryError{
            "Many-to-many pivot mutation requires a relational database backend"
        };
    }
}

template <
    typename Parent,
    typename Related
>
[[nodiscard]]
model::AttributeValue
pivot_parent_key(
    const Parent& parent,
    const BelongsToMany<Related>& relation
) {
    const auto key =
        parent.attribute_value(
            relation.parent_key()
        );

    if (!key) {
        throw QueryError{
            "Cannot mutate a many-to-many relation without an initialized parent key"
        };
    }

    return *key;
}

inline std::vector<
    model::AttributeValue
> unique_values(
    std::vector<
        model::AttributeValue
    > values
) {
    std::vector<
        model::AttributeValue
    > result;

    result.reserve(
        values.size()
    );

    for (
        auto& value :
        values
    ) {
        if (
            std::find(
                result.begin(),
                result.end(),
                value
            ) ==
            result.end()
        ) {
            result.push_back(
                std::move(value)
            );
        }
    }

    return result;
}

inline bool contains_value(
    const std::vector<
        model::AttributeValue
    >& values,
    const model::AttributeValue& value
) {
    return
        std::find(
            values.begin(),
            values.end(),
            value
        ) !=
        values.end();
}

inline database::Result
execute_pivot(
    database::Connection& connection,
    std::string_view connection_name,
    const CompiledQuery& compiled
) {
    report(
        QueryEvent{
            .connection =
                String{
                    connection_name
                },
            .backend =
                connection.backend(),
            .statement =
                compiled.text,
            .binding_count =
                compiled.bindings.size()
        }
    );

    return connection.execute(
        compiled.text,
        compiled.bindings
    );
}

template <typename Related>
std::size_t attach_pivot(
    database::Connection& connection,
    std::string_view connection_name,
    const model::AttributeValue& parent_key,
    const BelongsToMany<Related>& relation,
    const std::vector<
        model::AttributeValue
    >& related_keys
) {
    if (related_keys.empty()) {
        return 0;
    }

    std::vector<
        model::AttributeMap
    > rows;

    rows.reserve(
        related_keys.size()
    );

    for (
        const auto& related_key :
        related_keys
    ) {
        rows.push_back(
            model::AttributeMap{
                {
                    relation.foreign_pivot_key(),
                    parent_key
                },
                {
                    relation.related_pivot_key(),
                    related_key
                }
            }
        );
    }

    const auto compiled =
        compile_insert_many(
            relation.pivot_table(),
            rows,
            connection.backend()
        );

    return execute_pivot(
        connection,
        connection_name,
        compiled
    ).affected_rows;
}

template <typename Related>
std::size_t detach_pivot(
    database::Connection& connection,
    std::string_view connection_name,
    const model::AttributeValue& parent_key,
    const BelongsToMany<Related>& relation,
    const std::optional<
        std::vector<
            model::AttributeValue
        >
    >& related_keys
) {
    QueryPlan plan;

    plan.table =
        relation.pivot_table();

    plan.connection =
        String{
            connection_name
        };

    plan.predicates.push_back(
        Predicate{
            .kind =
                PredicateKind::comparison,
            .connector =
                BooleanConnector::and_,
            .column =
                relation.foreign_pivot_key(),
            .comparison =
                Comparison::equal,
            .values = {
                parent_key
            }
        }
    );

    if (related_keys) {
        if (related_keys->empty()) {
            return 0;
        }

        plan.predicates.push_back(
            Predicate{
                .kind =
                    PredicateKind::in_list,
                .connector =
                    BooleanConnector::and_,
                .column =
                    relation.related_pivot_key(),
                .comparison =
                    Comparison::equal,
                .values =
                    *related_keys
            }
        );
    }

    const auto compiled =
        compile_delete_where(
            plan,
            connection.backend()
        );

    return execute_pivot(
        connection,
        connection_name,
        compiled
    ).affected_rows;
}

template <typename Related>
[[nodiscard]]
std::vector<
    model::AttributeValue
> current_pivot_keys(
    database::Connection& connection,
    std::string_view connection_name,
    const model::AttributeValue& parent_key,
    const BelongsToMany<Related>& relation
) {
    QueryPlan plan;

    plan.table =
        relation.pivot_table();

    plan.connection =
        String{
            connection_name
        };

    plan.columns = {
        relation.related_pivot_key()
    };

    plan.predicates.push_back(
        Predicate{
            .kind =
                PredicateKind::comparison,
            .connector =
                BooleanConnector::and_,
            .column =
                relation.foreign_pivot_key(),
            .comparison =
                Comparison::equal,
            .values = {
                parent_key
            }
        }
    );

    const auto compiled =
        compile(
            plan,
            connection.backend()
        );

    const auto result =
        execute_pivot(
            connection,
            connection_name,
            compiled
        );

    std::vector<
        model::AttributeValue
    > values;

    for (
        const auto& row :
        result.rows
    ) {
        const auto found =
            row.find(
                relation.related_pivot_key()
            );

        if (
            found !=
            row.end() &&
            !contains_value(
                values,
                found->second
            )
        ) {
            values.push_back(
                found->second
            );
        }
    }

    return values;
}

} // namespace detail

template <
    typename Parent,
    typename Related
>
std::size_t attach(
    const Parent& parent,
    BelongsToMany<Related>& relation,
    std::vector<
        model::AttributeValue
    > related_keys
) {
    const auto connection_name =
        Parent::connection_name();

    auto connection =
        database::runtime::connection(
            connection_name
        );

    detail::ensure_pivot_backend(
        connection->backend()
    );

    const auto parent_key =
        detail::pivot_parent_key(
            parent,
            relation
        );

    const auto unique =
        detail::unique_values(
            std::move(
                related_keys
            )
        );

    const auto affected =
        detail::attach_pivot(
            *connection,
            connection_name,
            parent_key,
            relation,
            unique
        );

    if (affected != 0) {
        relation.unload();
    }

    return affected;
}

template <
    typename Parent,
    typename Related
>
std::size_t attach(
    const Parent& parent,
    BelongsToMany<Related>& relation,
    model::AttributeValue related_key
) {
    return attach(
        parent,
        relation,
        std::vector<
            model::AttributeValue
        >{
            std::move(
                related_key
            )
        }
    );
}

template <
    typename Parent,
    typename Related
>
std::size_t detach(
    const Parent& parent,
    BelongsToMany<Related>& relation
) {
    const auto connection_name =
        Parent::connection_name();

    auto connection =
        database::runtime::connection(
            connection_name
        );

    detail::ensure_pivot_backend(
        connection->backend()
    );

    const auto parent_key =
        detail::pivot_parent_key(
            parent,
            relation
        );

    const auto affected =
        detail::detach_pivot(
            *connection,
            connection_name,
            parent_key,
            relation,
            std::nullopt
        );

    if (affected != 0) {
        relation.unload();
    }

    return affected;
}

template <
    typename Parent,
    typename Related
>
std::size_t detach(
    const Parent& parent,
    BelongsToMany<Related>& relation,
    std::vector<
        model::AttributeValue
    > related_keys
) {
    const auto connection_name =
        Parent::connection_name();

    auto connection =
        database::runtime::connection(
            connection_name
        );

    detail::ensure_pivot_backend(
        connection->backend()
    );

    const auto parent_key =
        detail::pivot_parent_key(
            parent,
            relation
        );

    const auto unique =
        detail::unique_values(
            std::move(
                related_keys
            )
        );

    const auto affected =
        detail::detach_pivot(
            *connection,
            connection_name,
            parent_key,
            relation,
            unique
        );

    if (affected != 0) {
        relation.unload();
    }

    return affected;
}

template <
    typename Parent,
    typename Related
>
std::size_t detach(
    const Parent& parent,
    BelongsToMany<Related>& relation,
    model::AttributeValue related_key
) {
    return detach(
        parent,
        relation,
        std::vector<
            model::AttributeValue
        >{
            std::move(
                related_key
            )
        }
    );
}

template <
    typename Parent,
    typename Related
>
[[nodiscard]]
PivotSyncResult sync(
    const Parent& parent,
    BelongsToMany<Related>& relation,
    std::vector<
        model::AttributeValue
    > related_keys
) {
    const auto connection_name =
        Parent::connection_name();

    auto desired =
        detail::unique_values(
            std::move(
                related_keys
            )
        );

    const auto parent_key =
        detail::pivot_parent_key(
            parent,
            relation
        );

    if (
        database::runtime::manager()
            .backend(
                connection_name
            ) ==
        database::Backend::mongodb
    ) {
        throw QueryError{
            "Many-to-many pivot sync requires a relational database backend"
        };
    }

    auto transaction =
        database::runtime::manager()
            .transaction(
                connection_name
            );

    auto result =
        transaction.run(
            [&]() -> PivotSyncResult {
                auto& connection =
                    transaction.connection();

                detail::ensure_pivot_backend(
                    connection.backend()
                );

                const auto current =
                    detail::current_pivot_keys(
                        connection,
                        connection_name,
                        parent_key,
                        relation
                    );

                PivotSyncResult changes;

                for (
                    const auto& value :
                    desired
                ) {
                    if (
                        !detail::contains_value(
                            current,
                            value
                        )
                    ) {
                        changes.attached.push_back(
                            value
                        );
                    }
                }

                for (
                    const auto& value :
                    current
                ) {
                    if (
                        !detail::contains_value(
                            desired,
                            value
                        )
                    ) {
                        changes.detached.push_back(
                            value
                        );
                    }
                }

                if (
                    !changes.detached.empty()
                ) {
                    static_cast<void>(
                        detail::detach_pivot(
                            connection,
                            connection_name,
                            parent_key,
                            relation,
                            changes.detached
                        )
                    );
                }

                if (
                    !changes.attached.empty()
                ) {
                    static_cast<void>(
                        detail::attach_pivot(
                            connection,
                            connection_name,
                            parent_key,
                            relation,
                            changes.attached
                        )
                    );
                }

                return changes;
            }
        );

    if (result.changed()) {
        relation.unload();
    }

    return result;
}

} // namespace gungnir::orm
