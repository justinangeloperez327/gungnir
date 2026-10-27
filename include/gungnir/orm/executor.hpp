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
#include <gungnir/orm/query_log.hpp>
#include <gungnir/orm/relation_query.hpp>

namespace gungnir::orm {

namespace detail {

inline bool same_value(
    const model::AttributeValue& left,
    const model::AttributeValue& right
) {
    if (const auto* signed_value = std::get_if<Int64>(&left))
        if (const auto* unsigned_value = std::get_if<UInt64>(&right))
            return *signed_value >= 0 && static_cast<UInt64>(*signed_value) == *unsigned_value;
    if (const auto* unsigned_value = std::get_if<UInt64>(&left))
        if (const auto* signed_value = std::get_if<Int64>(&right))
            return *signed_value >= 0 && *unsigned_value == static_cast<UInt64>(*signed_value);
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
    const String& nested = {},
    const RelationConstraint* constraint = nullptr
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
        if (constraint) related_query.constrain(*constraint);
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
        if (constraint) related_query.constrain(*constraint);
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
        if (constraint) related_query.constrain(*constraint);
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
        report(QueryEvent{
            pivot_plan.connection,
            connection->backend(),
            compiled.text,
            compiled.bindings.size()
        });
        const auto pivot = connection->execute(
            compiled.text,
            compiled.bindings
        );
        connection.reset();

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
        if (constraint) related_query.constrain(*constraint);
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
        if (constraint) related_query.constrain(*constraint);
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
    const std::vector<String>& relations,
    const std::unordered_map<String,RelationConstraint>& constraints = {}
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
                    const auto constraint = constraints.find(root);
                    load_relation(models, descriptor, nested, constraint == constraints.end() ? nullptr : &constraint->second);
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

[[nodiscard]] inline String count_key(const model::AttributeValue& value) {
    if (const auto* key = std::get_if<Int64>(&value)) return "i" + std::to_string(*key);
    if (const auto* key = std::get_if<UInt64>(&value)) return "i" + std::to_string(*key);
    if (const auto* key = std::get_if<String>(&value)) return "s" + *key;
    throw ModelMetadataError{"Relationship counts require integer or string keys"};
}

// Counts use one aggregate query per relationship, independent of parent count.
template <typename ModelType>
void eager_count(Collection<ModelType>& models, const std::vector<String>& names) {
    if (models.empty()) return;
    for (const auto& name : names) {
        model::for_each_relation<ModelType>([&](const auto& descriptor) {
            if (descriptor.name != name) return;
            const auto& relation = models[0].*(descriptor.member);
            using Relation = std::remove_cvref_t<decltype(relation)>;
            const auto local = [&]() {
                if constexpr (relation_traits<Relation>::kind == model::RelationKind::belongs_to) return relation.foreign_key();
                else if constexpr (requires { relation.parent_key(); }) return relation.parent_key();
                else return relation.local_key();
            }();
            const auto keys = collect_values(models,local);
            for (auto& model : models) model.set_relation_count(name,0);
            if (keys.empty()) return;
            auto query = relation_query(models[0],relation);
            auto plan = query.plan();
            auto& filter = plan.predicates.back();
            filter.kind = PredicateKind::in_list;
            filter.values = keys;
            plan.count_group_column = filter.column;
            plan.groups = {filter.column};
            plan.columns.clear(); plan.orders.clear(); plan.eager_loads.clear(); plan.eager_counts.clear();
            plan.limit.reset(); plan.offset.reset();
            plan.aggregate = Aggregate{AggregateFunction::count,"*"};
            auto connection = database::runtime::connection(plan.connection);
            const auto compiled = orm::compile(plan,connection->backend());
            report(QueryEvent{.connection=plan.connection,.backend=connection->backend(),.statement=compiled.text,.binding_count=compiled.bindings.size()});
            const auto result = connection->execute(compiled.text,compiled.bindings);
            std::unordered_map<String,std::size_t> counts;
            for (const auto& row : result.rows) {
                const auto grouped = row.find("__gungnir_count_key"), count = row.find("aggregate");
                if (grouped == row.end() || count == row.end())
                    throw ModelMetadataError{"Relationship count result is missing its key or aggregate"};
                counts.insert_or_assign(count_key(grouped->second),model::value_cast<std::size_t>(count->second));
            }
            for (auto& model : models) {
                const auto key = model.attribute_value(local);
                if (!key || std::holds_alternative<std::nullptr_t>(*key) || std::holds_alternative<std::monostate>(*key)) continue;
                const auto found = counts.find(count_key(*key));
                if (found != counts.end()) model.set_relation_count(name,found->second);
            }
        });
    }
}

// Synchronous ORM operations cannot suspend while this cycle guard is active.
template <typename ModelType>
void touch_owners(const ModelType& instance) {
    if constexpr (requires { ModelType::touches; }) {
        const auto key = instance.primary_key_value();
        if (!key) return;
        static thread_local std::vector<model::AttributeValue> active;
        if (std::find(active.begin(),active.end(),*key) != active.end()) return;
        active.push_back(*key);
        struct Guard { std::vector<model::AttributeValue>& values; ~Guard() { values.pop_back(); } } guard{active};
        for (const auto name : ModelType::touches) {
            model::for_each_relation<ModelType>([&](const auto& descriptor) {
                if (descriptor.name != name) return;
                using Relation = std::remove_cvref_t<decltype(instance.*descriptor.member)>;
                if constexpr (relation_traits<Relation>::kind == model::RelationKind::belongs_to) {
                    auto owner = relation_query(instance,instance.*descriptor.member).without_eager_loads().first();
                    if (owner && !owner->touch()) throw ModelMetadataError{"Unable to touch parent model"};
                } else throw ModelMetadataError{"touches requires belongsTo relationships"};
            });
        }
    }
}

inline constexpr std::size_t max_eager_depth = 32;
inline thread_local std::size_t eager_depth = 0;

} // namespace detail

template <typename ModelType>
Collection<ModelType> Query<ModelType>::get() const {
    if (detail::eager_depth >= detail::max_eager_depth) throw ModelMetadataError{"Cyclic or excessively deep automatic eager loading"};
    struct DepthGuard { DepthGuard() { ++detail::eager_depth; } ~DepthGuard() { --detail::eager_depth; } } depth_guard;
    auto connection = database::runtime::connection(plan_.connection);
    const auto compiled = orm::compile(plan_, connection->backend());
    report(QueryEvent{
        .connection = plan_.connection,
        .backend = connection->backend(),
        .statement = compiled.text,
        .binding_count = compiled.bindings.size()
    });
    const auto result = connection->execute(
        compiled.text,
        compiled.bindings
    );

    auto models = Hydrator<ModelType>::many(result.rows);
    // A transaction scope retains its own lease. Ordinary queries must release
    // theirs before relation loading acquires another connection from the pool.
    connection.reset();
    detail::eager_load(models, plan_.eager_loads, plan_.eager_constraints);
    detail::eager_count(models, plan_.eager_counts);
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
        throw ModelNotFoundError{
            String{ModelType::table_name()}
        };
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
    validate_persisted_identity();
    auto connection = database::runtime::connection(
        connection_name()
    );

    const auto key_name = primary_key_name();

    if (!exists()) {
        if constexpr (uses_timestamps()) {
            const auto now = model::timestamp_now(timestamp_format());

            if (
                has_attribute(created_at_column()) &&
                !attribute_value(created_at_column())
            ) {
                force_fill({
                    {String{created_at_column()}, now}
                });
            }

            if (has_attribute(updated_at_column())) {
                force_fill({
                    {String{updated_at_column()}, now}
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
            connection->backend(),
            primary_key_incrementing() && !primary_key_value() ? key_name : std::string_view{}
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

        if (primary_key_incrementing() && !primary_key_value()) {
            throw ModelMetadataError{"Insert affected a row but the adapter returned no generated primary key"};
        }
        mark_persisted(true);
        clean();
        connection.reset();
        orm::detail::touch_owners(static_cast<const Derived&>(*this));
        return true;
    }

    std::optional<String> updated_timestamp;

    if constexpr (uses_timestamps()) {
        if (has_attribute(updated_at_column())) {
            updated_timestamp = model::timestamp_now(timestamp_format());
            force_fill({
                {String{updated_at_column()}, *updated_timestamp}
            });
        }
    }

    auto changes = dirty_attributes();
    changes.erase(String{key_name});

    if (updated_timestamp) {
        changes.insert_or_assign(
            String{updated_at_column()},
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
    connection.reset();
    orm::detail::touch_owners(static_cast<const Derived&>(*this));
    return true;
}

template <typename Derived>
bool Model<Derived>::update(const AttributeMap& values) {
    fill(values);
    return save();
}

template <typename Derived>
bool Model<Derived>::remove() {
    validate_persisted_identity();
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
        const auto values = model::soft_delete_attributes<Derived>(false);
        const auto compiled = orm::compile_update_where(plan,values,connection->backend());
        const auto result = connection->execute(
            compiled.text,
            compiled.bindings
        );

        if (result.affected_rows == 0) {
            return false;
        }

        for (const auto& [column,value] : values) {
            if (has_attribute(column)) {
                force_fill({{column,value}});
                sync_attribute(column);
            }
        }

        mark_soft_deleted(true);
        connection.reset();
        orm::detail::touch_owners(static_cast<const Derived&>(*this));
        return true;
    }

    return force_remove();
}

template <typename Derived>
bool Model<Derived>::force_remove() {
    validate_persisted_identity();
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
    connection.reset();
    orm::detail::touch_owners(static_cast<const Derived&>(*this));
    return true;
}

template <typename Derived>
bool Model<Derived>::restore() {
    validate_persisted_identity();
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
    const auto values = model::soft_delete_attributes<Derived>(true);
    const auto compiled = orm::compile_update_where(plan,values,connection->backend());
    const auto result = connection->execute(
        compiled.text,
        compiled.bindings
    );

    if (result.affected_rows == 0) {
        return false;
    }

    for (const auto& [column,value] : values) {
        if (has_attribute(column)) {
            force_fill({{column,value}});
            sync_attribute(column);
        }
    }

    mark_soft_deleted(false);
    connection.reset();
    orm::detail::touch_owners(static_cast<const Derived&>(*this));
    return true;
}

template <typename Derived>
bool Model<Derived>::touch() {
    validate_persisted_identity();
    if constexpr (!uses_timestamps()) {
        return false;
    }

    if (!exists() || !has_attribute(updated_at_column())) {
        return false;
    }

    force_fill({
        {String{updated_at_column()}, model::timestamp_now(timestamp_format())}
    });

    return save();
}

template <typename Derived>
std::optional<Derived> Model<Derived>::fresh() const {
    validate_persisted_identity();
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
    validate_persisted_identity();
    auto current = fresh();

    if (!current) {
        mark_missing();
        return false;
    }

    model::for_each_attribute<Derived>([&](const auto& descriptor) {
        derived().*(descriptor.member) = std::move(current.value().*(descriptor.member));
    });
    model::for_each_relation<Derived>([&](const auto& descriptor) {
        derived().*(descriptor.member) = std::move(current.value().*(descriptor.member));
    });
    auto& state = static_cast<Model<Derived>&>(*current);
    exists_ = state.exists_;
    recently_created_ = state.recently_created_;
    soft_deleted_ = state.soft_deleted_;
    persisted_key_ = std::move(state.persisted_key_);
    relation_counts_ = std::move(state.relation_counts_);
    return true;
}

template <typename Derived>
Derived Model<Derived>::replicate() const {
    Derived copy;
    auto values = attributes();
    // Replication copies user attributes, not identity or lifecycle state.
    // Newly inserted copies must generate their own timestamps and must not
    // inherit a source row's soft-deletion marker.
    values.erase(String{primary_key_name()});
    if constexpr (uses_timestamps()) {
        values.erase(String{created_at_column()});
        values.erase(String{updated_at_column()});
    }
    if constexpr (uses_soft_deletes()) {
        values.erase(String{deleted_at_column()});
    }
    copy.force_fill(values);
    auto& state = static_cast<Model<Derived>&>(copy);
    state.clear_attribute(primary_key_name());
    if constexpr (uses_timestamps()) {
        state.clear_attribute(created_at_column());
        state.clear_attribute(updated_at_column());
    }
    if constexpr (uses_soft_deletes()) state.clear_attribute(deleted_at_column());
    return copy;
}

} // namespace gungnir
