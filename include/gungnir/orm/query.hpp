#pragma once

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gungnir/model/model.hpp>
#include <gungnir/orm/collection.hpp>
#include <gungnir/orm/compiler.hpp>
#include <gungnir/orm/page.hpp>
#include <gungnir/orm/plan.hpp>

namespace gungnir::orm {

template <typename ModelType>
class Query {
public:
    Query() {
        plan_.table = String{ModelType::table_name()};
        plan_.connection = String{ModelType::connection_name()};

        if (plan_.table.empty()) {
            throw ModelMetadataError{
                "ORM query requires model table metadata"
            };
        }

        if constexpr (ModelType::uses_soft_deletes()) {
            plan_.predicates.push_back(Predicate{
                .kind = PredicateKind::is_null,
                .connector = BooleanConnector::and_,
                .column = String{ModelType::deleted_at_column()},
                .automatic = true
            });
        }
    }

    [[nodiscard]] const QueryPlan& plan() const noexcept {
        return plan_;
    }

    Query& select(std::initializer_list<String> columns) {
        plan_.columns.assign(columns.begin(), columns.end());
        return *this;
    }

    Query& add_select(String column) {
        plan_.columns.push_back(std::move(column));
        return *this;
    }

    Query& distinct(bool value = true) noexcept {
        plan_.distinct = value;
        return *this;
    }

    Query& where(
        String column,
        model::AttributeValue value
    ) {
        return where(
            std::move(column),
            Comparison::equal,
            std::move(value)
        );
    }

    Query& where(
        String column,
        Comparison comparison,
        model::AttributeValue value
    ) {
        add_comparison(
            BooleanConnector::and_,
            std::move(column),
            comparison,
            std::move(value)
        );
        return *this;
    }

    Query& or_where(
        String column,
        model::AttributeValue value
    ) {
        return or_where(
            std::move(column),
            Comparison::equal,
            std::move(value)
        );
    }

    Query& or_where(
        String column,
        Comparison comparison,
        model::AttributeValue value
    ) {
        add_comparison(
            BooleanConnector::or_,
            std::move(column),
            comparison,
            std::move(value)
        );
        return *this;
    }

    Query& where_column(
        String first,
        Comparison comparison,
        String second
    ) {
        plan_.predicates.push_back(Predicate{
            .kind = PredicateKind::column_comparison,
            .connector = BooleanConnector::and_,
            .column = std::move(first),
            .comparison = comparison,
            .other_column = std::move(second)
        });
        return *this;
    }

    Query& where_in(
        String column,
        std::vector<model::AttributeValue> values
    ) {
        add_list(
            PredicateKind::in_list,
            std::move(column),
            std::move(values)
        );
        return *this;
    }

    Query& where_not_in(
        String column,
        std::vector<model::AttributeValue> values
    ) {
        add_list(
            PredicateKind::not_in_list,
            std::move(column),
            std::move(values)
        );
        return *this;
    }

    Query& where_between(
        String column,
        model::AttributeValue lower,
        model::AttributeValue upper
    ) {
        add_list(
            PredicateKind::between,
            std::move(column),
            {std::move(lower), std::move(upper)}
        );
        return *this;
    }

    Query& where_not_between(
        String column,
        model::AttributeValue lower,
        model::AttributeValue upper
    ) {
        add_list(
            PredicateKind::not_between,
            std::move(column),
            {std::move(lower), std::move(upper)}
        );
        return *this;
    }

    Query& where_null(String column) {
        add_simple(PredicateKind::is_null, std::move(column));
        return *this;
    }

    Query& where_not_null(String column) {
        add_simple(PredicateKind::is_not_null, std::move(column));
        return *this;
    }

    Query& join(
        String table,
        String first,
        Comparison comparison,
        String second
    ) {
        return add_join(
            JoinType::inner,
            std::move(table),
            std::move(first),
            comparison,
            std::move(second)
        );
    }

    Query& left_join(
        String table,
        String first,
        Comparison comparison,
        String second
    ) {
        return add_join(
            JoinType::left,
            std::move(table),
            std::move(first),
            comparison,
            std::move(second)
        );
    }

    Query& right_join(
        String table,
        String first,
        Comparison comparison,
        String second
    ) {
        return add_join(
            JoinType::right,
            std::move(table),
            std::move(first),
            comparison,
            std::move(second)
        );
    }

    Query& cross_join(String table) {
        plan_.joins.push_back(Join{
            .type = JoinType::cross,
            .table = std::move(table)
        });
        return *this;
    }

    Query& group_by(std::initializer_list<String> columns) {
        plan_.groups.insert(
            plan_.groups.end(),
            columns.begin(),
            columns.end()
        );
        return *this;
    }

    Query& having(
        String column,
        Comparison comparison,
        model::AttributeValue value
    ) {
        plan_.having.push_back(Predicate{
            .kind = PredicateKind::comparison,
            .connector = BooleanConnector::and_,
            .column = std::move(column),
            .comparison = comparison,
            .values = {std::move(value)}
        });
        return *this;
    }

    Query& order_by(
        String column,
        SortDirection direction = SortDirection::asc
    ) {
        plan_.orders.push_back(Order{
            .column = std::move(column),
            .direction = direction
        });
        return *this;
    }

    Query& order_by_desc(String column) {
        return order_by(std::move(column), SortDirection::desc);
    }

    Query& latest(String column = "created_at") {
        return order_by(std::move(column), SortDirection::desc);
    }

    Query& oldest(String column = "created_at") {
        return order_by(std::move(column), SortDirection::asc);
    }

    Query& lock_for_update() noexcept {
        plan_.lock = LockMode::for_update;
        return *this;
    }

    Query& shared_lock() noexcept {
        plan_.lock = LockMode::shared;
        return *this;
    }

    Query& limit(std::size_t value) noexcept {
        plan_.limit = value;
        return *this;
    }

    Query& take(std::size_t value) noexcept {
        return limit(value);
    }

    Query& offset(std::size_t value) noexcept {
        plan_.offset = value;
        return *this;
    }

    Query& skip(std::size_t value) noexcept {
        return offset(value);
    }

    template <typename Callback>
    Query& when(bool condition, Callback&& callback) {
        if (condition) {
            std::invoke(std::forward<Callback>(callback), *this);
        }
        return *this;
    }

    template <typename Callback>
    Query& tap(Callback&& callback) {
        std::invoke(std::forward<Callback>(callback), *this);
        return *this;
    }

    template <typename Callback>
    Query& scope(Callback&& callback) {
        return tap(std::forward<Callback>(callback));
    }

    Query& with(String relation) {
        bool known = false;
        const auto dot = relation.find('.');
        const auto root = dot == String::npos
            ? relation
            : relation.substr(0, dot);

        model::for_each_relation<ModelType>([&](const auto& descriptor) {
            if (descriptor.name == root) {
                known = true;
            }
        });

        if (!known) {
            throw ModelMetadataError{
                "Unknown eager-load relation '" + relation + "'"
            };
        }

        if (
            std::find(
                plan_.eager_loads.begin(),
                plan_.eager_loads.end(),
                relation
            ) == plan_.eager_loads.end()
        ) {
            plan_.eager_loads.push_back(std::move(relation));
        }

        return *this;
    }

    Query& with(std::initializer_list<String> relations) {
        for (const auto& relation : relations) {
            with(relation);
        }
        return *this;
    }

    Query& with_deleted() {
        if constexpr (ModelType::uses_soft_deletes()) {
            erase_soft_delete_scope();
        }
        return *this;
    }

    Query& only_deleted() {
        if constexpr (ModelType::uses_soft_deletes()) {
            erase_soft_delete_scope();
            where_not_null(String{ModelType::deleted_at_column()});
        }
        return *this;
    }

    Query& where_key(model::AttributeValue key) {
        return where(
            String{ModelType::primary_key_name()},
            std::move(key)
        ).limit(1);
    }

    Query& where_key_in(std::vector<model::AttributeValue> keys) {
        return where_in(
            String{ModelType::primary_key_name()},
            std::move(keys)
        );
    }

    [[nodiscard]] CompiledQuery compile(
        database::Backend backend
    ) const {
        return orm::compile(plan_, backend);
    }

    [[nodiscard]] Collection<ModelType> get() const;
    [[nodiscard]] std::optional<ModelType> first() const;
    [[nodiscard]] ModelType first_or_fail() const;
    [[nodiscard]] std::vector<model::AttributeValue> pluck(
        String column
    ) const;
    [[nodiscard]] std::optional<model::AttributeValue> value(
        String column
    ) const;

    [[nodiscard]] std::size_t count() const;
    [[nodiscard]] bool exists() const;
    [[nodiscard]] Double sum(String column) const;
    [[nodiscard]] Double average(String column) const;
    [[nodiscard]] std::optional<model::AttributeValue> min(
        String column
    ) const;
    [[nodiscard]] std::optional<model::AttributeValue> max(
        String column
    ) const;

    [[nodiscard]] Page<ModelType> paginate(
        std::size_t page = 1,
        std::size_t per_page = 15
    ) const;

    std::size_t update(const model::AttributeMap& values) const;
    std::size_t remove() const;
    std::size_t force_remove() const;
    std::size_t restore() const;
    std::size_t increment(String column, Double amount = 1.0) const;
    std::size_t decrement(String column, Double amount = 1.0) const;

    bool insert(const model::AttributeMap& values) const;
    std::size_t insert_many(
        const std::vector<model::AttributeMap>& rows
    ) const;

    std::size_t upsert(
        const std::vector<model::AttributeMap>& rows,
        std::vector<String> unique_by,
        std::vector<String> update_columns = {}
    ) const;

    std::size_t insert_or_ignore(
        const std::vector<model::AttributeMap>& rows,
        std::vector<String> unique_by
    ) const;

    template <typename Callback>
    bool chunk(std::size_t size, Callback&& callback) const {
        if (size == 0) {
            throw std::invalid_argument(
                "ORM chunk size must be greater than zero"
            );
        }

        const auto base_offset = plan_.offset.value_or(0);
        const auto total_limit = plan_.limit;
        std::size_t processed = 0;

        while (true) {
            if (total_limit && processed >= *total_limit) {
                return true;
            }

            const auto batch_size = total_limit
                ? std::min(size, *total_limit - processed)
                : size;

            auto query = *this;
            query.plan_.limit = batch_size;
            query.plan_.offset = base_offset + processed;

            auto models = query.get();
            if (models.empty()) {
                return true;
            }

            if constexpr (
                std::is_same_v<
                    std::invoke_result_t<Callback&, Collection<ModelType>&>,
                    bool
                >
            ) {
                if (!std::invoke(callback, models)) {
                    return false;
                }
            } else {
                std::invoke(callback, models);
            }

            processed += models.size();

            if (models.size() < batch_size) {
                return true;
            }
        }
    }

    template <typename Callback>
    bool each(Callback&& callback, std::size_t chunk_size = 1000) const {
        return chunk(
            chunk_size,
            [&](Collection<ModelType>& models) {
                for (auto& model : models) {
                    if constexpr (
                        std::is_same_v<
                            std::invoke_result_t<Callback&, ModelType&>,
                            bool
                        >
                    ) {
                        if (!std::invoke(callback, model)) {
                            return false;
                        }
                    } else {
                        std::invoke(callback, model);
                    }
                }

                return true;
            }
        );
    }

private:
    void erase_soft_delete_scope() {
        std::erase_if(
            plan_.predicates,
            [](const Predicate& predicate) {
                return predicate.automatic;
            }
        );
    }

    Query& add_join(
        JoinType type,
        String table,
        String first,
        Comparison comparison,
        String second
    ) {
        plan_.joins.push_back(Join{
            .type = type,
            .table = std::move(table),
            .first = std::move(first),
            .comparison = comparison,
            .second = std::move(second)
        });
        return *this;
    }

    void add_comparison(
        BooleanConnector connector,
        String column,
        Comparison comparison,
        model::AttributeValue value
    ) {
        plan_.predicates.push_back(Predicate{
            .kind = PredicateKind::comparison,
            .connector = connector,
            .column = std::move(column),
            .comparison = comparison,
            .values = {std::move(value)}
        });
    }

    void add_list(
        PredicateKind kind,
        String column,
        std::vector<model::AttributeValue> values
    ) {
        plan_.predicates.push_back(Predicate{
            .kind = kind,
            .connector = BooleanConnector::and_,
            .column = std::move(column),
            .comparison = Comparison::equal,
            .values = std::move(values)
        });
    }

    void add_simple(
        PredicateKind kind,
        String column
    ) {
        plan_.predicates.push_back(Predicate{
            .kind = kind,
            .connector = BooleanConnector::and_,
            .column = std::move(column)
        });
    }

    QueryPlan plan_;
};

} // namespace gungnir::orm

namespace gungnir {

template <typename Derived>
orm::Query<Derived> Model<Derived>::query() {
    return orm::Query<Derived>{};
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::select(
    std::initializer_list<String> columns
) {
    return query().select(columns);
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::distinct(bool value) {
    return query().distinct(value);
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::where(
    String column,
    model::AttributeValue value
) {
    return query().where(
        std::move(column),
        std::move(value)
    );
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::where(
    String column,
    orm::Comparison comparison,
    model::AttributeValue value
) {
    return query().where(
        std::move(column),
        comparison,
        std::move(value)
    );
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::or_where(
    String column,
    model::AttributeValue value
) {
    return query().or_where(
        std::move(column),
        std::move(value)
    );
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::or_where(
    String column,
    orm::Comparison comparison,
    model::AttributeValue value
) {
    return query().or_where(
        std::move(column),
        comparison,
        std::move(value)
    );
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::where_column(
    String first,
    orm::Comparison comparison,
    String second
) {
    return query().where_column(
        std::move(first),
        comparison,
        std::move(second)
    );
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::where_in(
    String column,
    std::vector<model::AttributeValue> values
) {
    return query().where_in(std::move(column), std::move(values));
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::where_not_in(
    String column,
    std::vector<model::AttributeValue> values
) {
    return query().where_not_in(std::move(column), std::move(values));
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::where_between(
    String column,
    model::AttributeValue lower,
    model::AttributeValue upper
) {
    return query().where_between(
        std::move(column),
        std::move(lower),
        std::move(upper)
    );
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::where_not_between(
    String column,
    model::AttributeValue lower,
    model::AttributeValue upper
) {
    return query().where_not_between(
        std::move(column),
        std::move(lower),
        std::move(upper)
    );
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::where_null(String column) {
    return query().where_null(std::move(column));
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::where_not_null(String column) {
    return query().where_not_null(std::move(column));
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::order_by(String column) {
    return query().order_by(std::move(column));
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::order_by_desc(String column) {
    return query().order_by_desc(std::move(column));
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::latest(String column) {
    return query().latest(std::move(column));
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::oldest(String column) {
    return query().oldest(std::move(column));
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::with(String relation) {
    return query().with(std::move(relation));
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::with(
    std::initializer_list<String> relations
) {
    return query().with(relations);
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::limit(std::size_t value) {
    return query().limit(value);
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::take(std::size_t value) {
    return query().take(value);
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::offset(std::size_t value) {
    return query().offset(value);
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::skip(std::size_t value) {
    return query().skip(value);
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::with_deleted() {
    return query().with_deleted();
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::only_deleted() {
    return query().only_deleted();
}

} // namespace gungnir
