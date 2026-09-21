#pragma once

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <gungnir/database/connection.hpp>
#include <gungnir/database/runtime.hpp>
#include <gungnir/model/timestamps.hpp>
#include <gungnir/orm/collection.hpp>
#include <gungnir/orm/hydrator.hpp>
#include <gungnir/orm/mutation.hpp>
#include <gungnir/orm/advanced_mutation.hpp>
#include <gungnir/orm/query.hpp>

namespace gungnir::orm {

namespace detail {

inline bool same_value(
    const model::AttributeValue& left,
    const model::AttributeValue& right
) {
    return left == right;
}

inline void push_unique(
    std::vector<model::AttributeValue>& values,
    const model::AttributeValue& value
) {
    if (
        std::find(values.begin(), values.end(), value) == values.end()
    ) {
        values.push_back(value);
    }
}

template <typename Relation>
struct relation_traits;

template <typename Related>
struct relation_traits<HasOne<Related>> {
    using related_type = Related;
    static constexpr model::RelationKind kind = model::RelationKind::has_one;
};

template <typename Related>
struct relation_traits<HasMany<Related>> {
    using related_type = Related;
    static constexpr model::RelationKind kind = model::RelationKind::has_many;
};

template <typename Related>
struct relation_traits<BelongsTo<Related>> {
    using related_type = Related;
    static constexpr model::RelationKind kind = model::RelationKind::belongs_to;
};

template <typename Related>
struct relation_traits<BelongsToMany<Related>> {
    using related_type = Related;
    static constexpr model::RelationKind kind =
        model::RelationKind::belongs_to_many;
};

template <typename Related, typename Through>
struct relation_traits<HasOneThrough<Related, Through>> {
    using related_type = Related;
    using through_type = Through;
    static constexpr model::RelationKind kind =
        model::RelationKind::has_one_through;
};

template <typename Related, typename Through>
struct relation_traits<HasManyThrough<Related, Through>> {
    using related_type = Related;
    using through_type = Through;
    static constexpr model::RelationKind kind =
        model::RelationKind::has_many_through;
};

template <typename ModelType>
std::vector<model::AttributeValue> collect_values(
    const Collection<ModelType>& models,
    std::string_view attribute
) {
    std::vector<model::AttributeValue> values;

    for (const auto& item : models) {
        const auto value = item.attribute_value(attribute);
        if (value) {
            push_unique(values, *value);
        }
    }

    return values;
}

template <typename Parent, typename Descriptor>
void load_relation(
    Collection<Parent>& parents,
    const Descriptor& descriptor,
    const String& nested = {}
) {
    if (parents.empty()) {
        return;
    }

    using Relation = std::remove_cvref_t<
        decltype(std::declval<Parent>().*(descriptor.member))
    >;
    using Traits = relation_traits<Relation>;
    using Related = typename Traits::related_type;

    auto& sample = parents.first().*(descriptor.member);

    if constexpr (Traits::kind == model::RelationKind::has_many) {
        const auto parent_keys = collect_values(
            parents,
            sample.local_key()
        );

        auto related_query = Query<Related>{}
            .where_in(sample.foreign_key(), parent_keys);
        if (!nested.empty()) {
            related_query.with(nested);
        }
        auto related = related_query.get();

        for (auto& parent : parents) {
            std::vector<Related> matches;
            const auto parent_key = parent.attribute_value(sample.local_key());

            if (parent_key) {
                for (const auto& item : related) {
                    const auto foreign =
                        item.attribute_value(sample.foreign_key());

                    if (foreign && same_value(*foreign, *parent_key)) {
                        matches.push_back(item);
                    }
                }
            }

            (parent.*(descriptor.member)).set(std::move(matches));
        }
    } else if constexpr (Traits::kind == model::RelationKind::has_one) {
        const auto parent_keys = collect_values(
            parents,
            sample.local_key()
        );

        auto related_query = Query<Related>{}
            .where_in(sample.foreign_key(), parent_keys);
        if (!nested.empty()) {
            related_query.with(nested);
        }
        auto related = related_query.get();

        for (auto& parent : parents) {
            const auto parent_key = parent.attribute_value(sample.local_key());
            bool matched = false;

            if (parent_key) {
                for (const auto& item : related) {
                    const auto foreign =
                        item.attribute_value(sample.foreign_key());

                    if (foreign && same_value(*foreign, *parent_key)) {
                        (parent.*(descriptor.member)).set(item);
                        matched = true;
                        break;
                    }
                }
            }

            if (!matched) {
                (parent.*(descriptor.member)).clear();
            }
        }
    } else if constexpr (Traits::kind == model::RelationKind::belongs_to) {
        const auto foreign_keys = collect_values(
            parents,
            sample.foreign_key()
        );

        auto related_query = Query<Related>{}
            .where_in(sample.owner_key(), foreign_keys);
        if (!nested.empty()) {
            related_query.with(nested);
        }
        auto related = related_query.get();

        for (auto& parent : parents) {
            const auto foreign =
                parent.attribute_value(sample.foreign_key());
            bool matched = false;

            if (foreign) {
                for (const auto& item : related) {
                    const auto owner =
                        item.attribute_value(sample.owner_key());

                    if (owner && same_value(*owner, *foreign)) {
                        (parent.*(descriptor.member)).set(item);
                        matched = true;
                        break;
                    }
                }
            }

            if (!matched) {
                (parent.*(descriptor.member)).clear();
            }
        }
    } else if constexpr (
        Traits::kind == model::RelationKind::belongs_to_many
    ) {
        const auto parent_keys = collect_values(
            parents,
            sample.parent_key()
        );

        QueryPlan pivot_plan;
        pivot_plan.table = sample.pivot_table();
        pivot_plan.connection = String{Parent::connection_name()};
        pivot_plan.columns = {
            sample.foreign_pivot_key(),
            sample.related_pivot_key()
        };
        pivot_plan.predicates.push_back(Predicate{
            .kind = PredicateKind::in_list,
            .connector = BooleanConnector::and_,
            .column = sample.foreign_pivot_key(),
            .comparison = Comparison::equal,
            .values = parent_keys
        });

        auto connection = database::runtime::connection(
            pivot_plan.connection
        );
        const auto compiled = orm::compile(
            pivot_plan,
            connection->backend()
        );
        const auto pivot = connection->execute(
            compiled.text,
            compiled.bindings
        );

        std::vector<model::AttributeValue> related_keys;
        for (const auto& row : pivot.rows) {
            const auto found = row.find(sample.related_pivot_key());
            if (found != row.end()) {
                push_unique(related_keys, found->second);
            }
        }

        auto related_query = Query<Related>{}
            .where_in(sample.related_key(), related_keys);
        if (!nested.empty()) {
            related_query.with(nested);
        }
        auto related = related_query.get();

        for (auto& parent : parents) {
            std::vector<Related> matches;
            const auto parent_key =
                parent.attribute_value(sample.parent_key());

            if (parent_key) {
                for (const auto& row : pivot.rows) {
                    const auto parent_found =
                        row.find(sample.foreign_pivot_key());
                    const auto related_found =
                        row.find(sample.related_pivot_key());

                    if (
                        parent_found == row.end() ||
                        related_found == row.end() ||
                        !same_value(parent_found->second, *parent_key)
                    ) {
                        continue;
                    }

                    for (const auto& item : related) {
                        const auto key =
                            item.attribute_value(sample.related_key());

                        if (
                            key &&
                            same_value(*key, related_found->second)
                        ) {
                            matches.push_back(item);
                        }
                    }
                }
            }

            (parent.*(descriptor.member)).set(std::move(matches));
        }
    } else {
        using Through = typename Traits::through_type;

        const auto parent_keys = collect_values(
            parents,
            sample.local_key()
        );

        auto through = Query<Through>{}
            .where_in(sample.first_key(), parent_keys)
            .get();

        const auto through_keys = collect_values(
            through,
            sample.second_local_key()
        );

        auto related_query = Query<Related>{}
            .where_in(sample.second_key(), through_keys);
        if (!nested.empty()) {
            related_query.with(nested);
        }
        auto related = related_query.get();

        for (auto& parent : parents) {
            std::vector<Related> matches;
            const auto parent_key =
                parent.attribute_value(sample.local_key());

            if (parent_key) {
                for (const auto& bridge : through) {
                    const auto bridge_parent =
                        bridge.attribute_value(sample.first_key());
                    const auto bridge_key =
                        bridge.attribute_value(sample.second_local_key());

                    if (
                        !bridge_parent ||
                        !bridge_key ||
                        !same_value(*bridge_parent, *parent_key)
                    ) {
                        continue;
                    }

                    for (const auto& item : related) {
                        const auto related_bridge =
                            item.attribute_value(sample.second_key());

                        if (
                            related_bridge &&
                            same_value(*related_bridge, *bridge_key)
                        ) {
                            matches.push_back(item);
                        }
                    }
                }
            }

            if constexpr (
                Traits::kind == model::RelationKind::has_one_through
            ) {
                if (matches.empty()) {
                    (parent.*(descriptor.member)).clear();
                } else {
                    (parent.*(descriptor.member)).set(
                        std::move(matches.front())
                    );
                }
            } else {
                (parent.*(descriptor.member)).set(std::move(matches));
            }
        }
    }
}

template <typename ModelType>
void eager_load(
    Collection<ModelType>& models,
    const std::vector<String>& relations
) {
    if (models.empty() || relations.empty()) {
        return;
    }

    for (const auto& requested : relations) {
        bool found = false;
        const auto dot = requested.find('.');
        const auto root = dot == String::npos
            ? requested
            : requested.substr(0, dot);
        const auto nested = dot == String::npos
            ? String{}
            : requested.substr(dot + 1);

        model::for_each_relation<ModelType>(
            [&](const auto& descriptor) {
                if (descriptor.name == root) {
                    found = true;
                    load_relation(models, descriptor, nested);
                }
            }
        );

        if (!found) {
            throw ModelMetadataError{
                "Unknown eager-load relation '" + requested + "'"
            };
        }
    }
}

} // namespace detail

template <typename ModelType>
Collection<ModelType> Query<ModelType>::get() const {
    auto connection = database::runtime::connection(plan_.connection);
    const auto compiled = orm::compile(plan_, connection->backend());
    const auto result = connection->execute(
        compiled.text,
        compiled.bindings
    );

    auto models = Hydrator<ModelType>::many(result.rows);
    detail::eager_load(models, plan_.eager_loads);
    return models;
}

template <typename ModelType>
std::optional<ModelType> Query<ModelType>::first() const {
    auto copy = *this;
    copy.plan_.limit = 1;

    auto models = copy.get();
    if (models.empty()) {
        return std::nullopt;
    }

    return models.first();
}

template <typename ModelType>
ModelType Query<ModelType>::first_or_fail() const {
    auto result = first();

    if (!result) {
        throw std::out_of_range("Gungnir ORM model was not found");
    }

    return std::move(*result);
}

} // namespace gungnir::orm

namespace gungnir {

template <typename Derived>
orm::Collection<Derived> Model<Derived>::all() {
    return query().get();
}

template <typename Derived>
std::optional<Derived> Model<Derived>::first() {
    return query().first();
}

template <typename Derived>
Derived Model<Derived>::first_or_fail() {
    return query().first_or_fail();
}

template <typename Derived>
std::size_t Model<Derived>::count() {
    return query().count();
}

template <typename Derived>
orm::Page<Derived> Model<Derived>::paginate(
    std::size_t page,
    std::size_t per_page
) {
    return query().paginate(page, per_page);
}

template <typename Derived>
std::optional<Derived> Model<Derived>::find(
    model::AttributeValue key
) {
    return query().where_key(std::move(key)).first();
}

template <typename Derived>
Derived Model<Derived>::create(const AttributeMap& values) {
    Derived instance;
    instance.fill(values);

    if (!instance.save()) {
        throw std::runtime_error(
            "Gungnir ORM insert did not affect a row"
        );
    }

    return instance;
}

template <typename Derived>
bool Model<Derived>::save() {
    auto connection = database::runtime::connection(
        connection_name()
    );

    const auto key_name = primary_key_name();

    if (!exists()) {
        if constexpr (uses_timestamps()) {
            const auto now = model::timestamp_now();

            if (
                has_attribute("created_at") &&
                !attribute_value("created_at")
            ) {
                force_fill({
                    {"created_at", now}
                });
            }

            if (has_attribute("updated_at")) {
                force_fill({
                    {"updated_at", now}
                });
            }
        }

        auto values = attributes();

        if (primary_key_incrementing()) {
            const auto key = primary_key_value();
            if (!key) {
                values.erase(String{key_name});
            }
        } else if (!primary_key_value()) {
            throw ModelMetadataError{
                "Non-incrementing model requires a primary key before save()"
            };
        }

        const auto compiled = orm::compile_insert(
            table_name(),
            values,
            connection->backend()
        );

        auto result = connection->execute(
            compiled.text,
            compiled.bindings
        );

        if (result.affected_rows == 0) {
            return false;
        }

        if (
            primary_key_incrementing() &&
            result.inserted_id &&
            !primary_key_value()
        ) {
            force_fill({
                {String{key_name}, *result.inserted_id}
            });
        }

        mark_persisted(true);
        clean();
        return true;
    }

    std::optional<String> updated_timestamp;

    if constexpr (uses_timestamps()) {
        if (has_attribute("updated_at")) {
            updated_timestamp = model::timestamp_now();
            force_fill({
                {"updated_at", *updated_timestamp}
            });
        }
    }

    auto changes = dirty_attributes();
    changes.erase(String{key_name});

    if (updated_timestamp) {
        changes.insert_or_assign(
            "updated_at",
            *updated_timestamp
        );
    }

    if (changes.empty()) {
        clear_recently_created();
        return true;
    }

    const auto key_value = primary_key_value();
    if (!key_value) {
        throw ModelMetadataError{
            "Persisted model is missing its primary key"
        };
    }

    const auto compiled = orm::compile_update(
        table_name(),
        changes,
        key_name,
        *key_value,
        connection->backend()
    );

    const auto result = connection->execute(
        compiled.text,
        compiled.bindings
    );

    if (result.affected_rows == 0) {
        return false;
    }

    clean();
    clear_recently_created();
    return true;
}

template <typename Derived>
bool Model<Derived>::update(const AttributeMap& values) {
    fill(values);
    return save();
}

template <typename Derived>
bool Model<Derived>::remove() {
    if (!exists()) {
        return false;
    }

    if constexpr (uses_soft_deletes()) {
        const auto key_value = primary_key_value();
        if (!key_value) {
            throw ModelMetadataError{
                "Persisted model is missing its primary key"
            };
        }

        orm::QueryPlan plan;
        plan.table = String{table_name()};
        plan.connection = String{connection_name()};
        plan.predicates.push_back(orm::Predicate{
            .kind = orm::PredicateKind::comparison,
            .column = String{primary_key_name()},
            .values = {*key_value}
        });

        auto connection = database::runtime::connection(
            connection_name()
        );
        const auto compiled = orm::compile_soft_delete_where(
            plan,
            String{deleted_at_column()},
            false,
            connection->backend()
        );
        const auto result = connection->execute(
            compiled.text,
            compiled.bindings
        );

        if (result.affected_rows == 0) {
            return false;
        }

        if (has_attribute(deleted_at_column())) {
            force_fill({
                {
                    String{deleted_at_column()},
                    model::timestamp_now()
                }
            });
            sync_attribute(deleted_at_column());
        }

        mark_soft_deleted(true);
        return true;
    }

    return force_remove();
}

template <typename Derived>
bool Model<Derived>::force_remove() {
    if (!exists()) {
        return false;
    }

    const auto key_value = primary_key_value();
    if (!key_value) {
        throw ModelMetadataError{
            "Persisted model is missing its primary key"
        };
    }

    auto connection = database::runtime::connection(
        connection_name()
    );

    const auto compiled = orm::compile_delete(
        table_name(),
        primary_key_name(),
        *key_value,
        connection->backend()
    );

    const auto result = connection->execute(
        compiled.text,
        compiled.bindings
    );

    if (result.affected_rows == 0) {
        return false;
    }

    mark_missing();
    mark_soft_deleted(false);
    return true;
}

template <typename Derived>
bool Model<Derived>::restore() {
    if constexpr (!uses_soft_deletes()) {
        return false;
    }

    if (!exists() || !trashed()) {
        return false;
    }

    const auto key_value = primary_key_value();
    if (!key_value) {
        throw ModelMetadataError{
            "Persisted model is missing its primary key"
        };
    }

    orm::QueryPlan plan;
    plan.table = String{table_name()};
    plan.connection = String{connection_name()};
    plan.predicates.push_back(orm::Predicate{
        .kind = orm::PredicateKind::comparison,
        .column = String{primary_key_name()},
        .values = {*key_value}
    });

    auto connection = database::runtime::connection(
        connection_name()
    );
    const auto compiled = orm::compile_soft_delete_where(
        plan,
        String{deleted_at_column()},
        true,
        connection->backend()
    );
    const auto result = connection->execute(
        compiled.text,
        compiled.bindings
    );

    if (result.affected_rows == 0) {
        return false;
    }

    if (has_attribute(deleted_at_column())) {
        force_fill({
            {String{deleted_at_column()}, nullptr}
        });
        sync_attribute(deleted_at_column());
    }

    mark_soft_deleted(false);
    return true;
}

template <typename Derived>
bool Model<Derived>::touch() {
    if constexpr (!uses_timestamps()) {
        return false;
    }

    if (!exists() || !has_attribute("updated_at")) {
        return false;
    }

    force_fill({
        {"updated_at", model::timestamp_now()}
    });

    return save();
}

template <typename Derived>
std::optional<Derived> Model<Derived>::fresh() const {
    if (!exists()) {
        return std::nullopt;
    }

    const auto key = primary_key_value();
    if (!key) {
        return std::nullopt;
    }

    auto builder = query();
    if constexpr (uses_soft_deletes()) {
        builder.with_deleted();
    }

    return builder.where_key(*key).first();
}

template <typename Derived>
bool Model<Derived>::refresh() {
    auto current = fresh();

    if (!current) {
        mark_missing();
        return false;
    }

    force_fill(current->attributes());
    clean();
    mark_persisted(false);
    mark_soft_deleted(current->trashed());
    unload_relations();
    return true;
}

template <typename Derived>
Derived Model<Derived>::replicate() const {
    Derived copy;
    auto values = attributes();
    values.erase(String{primary_key_name()});
    copy.force_fill(values);
    return copy;
}

} // namespace gungnir
