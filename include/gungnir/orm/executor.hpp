#pragma once

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <gungnir/database/runtime.hpp>
#include <gungnir/orm/collection.hpp>
#include <gungnir/orm/hydrator.hpp>
#include <gungnir/orm/mutation.hpp>
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
    const Descriptor& descriptor
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

        auto related = Query<Related>{}
            .where_in(sample.foreign_key(), parent_keys)
            .get();

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

        auto related = Query<Related>{}
            .where_in(sample.foreign_key(), parent_keys)
            .get();

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

        auto related = Query<Related>{}
            .where_in(sample.owner_key(), foreign_keys)
            .get();

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
            .kind = PredicateKind::in,
            .connector = BooleanConnector::and_,
            .column = sample.foreign_pivot_key(),
            .comparison = Operator::equal,
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

        auto related = Query<Related>{}
            .where_in(sample.related_key(), related_keys)
            .get();

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

        auto related = Query<Related>{}
            .where_in(sample.second_key(), through_keys)
            .get();

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

        model::for_each_relation<ModelType>(
            [&](const auto& descriptor) {
                if (descriptor.name == requested) {
                    found = true;
                    load_relation(models, descriptor);
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
std::optional<Derived> Model<Derived>::find(
    model::AttributeValue key
) {
    return query().find(std::move(key)).first();
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

    auto changes = dirty_attributes();
    changes.erase(String{key_name});

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
    return true;
}

} // namespace gungnir
