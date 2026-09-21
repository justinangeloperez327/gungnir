#pragma once

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <gungnir/model/model.hpp>
#include <gungnir/orm/collection.hpp>
#include <gungnir/orm/compiler.hpp>
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
    }

    [[nodiscard]] const QueryPlan& plan() const noexcept {
        return plan_;
    }

    Query& select(std::initializer_list<String> columns) {
        plan_.columns.assign(columns.begin(), columns.end());
        return *this;
    }

    Query& where_(
        String column,
        model::AttributeValue value
    ) {
        return where_(
            std::move(column),
            Operator::equal,
            std::move(value)
        );
    }

    Query& where_(
        String column,
        Operator comparison,
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
            Operator::equal,
            std::move(value)
        );
    }

    Query& or_where(
        String column,
        Operator comparison,
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

    Query& where_in(
        String column,
        std::vector<model::AttributeValue> values
    ) {
        plan_.predicates.push_back(Predicate{
            .kind = PredicateKind::in,
            .connector = BooleanConnector::and_,
            .column = std::move(column),
            .comparison = Operator::equal,
            .values = std::move(values)
        });
        return *this;
    }

    Query& where_not_in(
        String column,
        std::vector<model::AttributeValue> values
    ) {
        plan_.predicates.push_back(Predicate{
            .kind = PredicateKind::not_in,
            .connector = BooleanConnector::and_,
            .column = std::move(column),
            .comparison = Operator::equal,
            .values = std::move(values)
        });
        return *this;
    }

    Query& where_null(String column) {
        plan_.predicates.push_back(Predicate{
            .kind = PredicateKind::null_,
            .connector = BooleanConnector::and_,
            .column = std::move(column)
        });
        return *this;
    }

    Query& where_not_null(String column) {
        plan_.predicates.push_back(Predicate{
            .kind = PredicateKind::not_null,
            .connector = BooleanConnector::and_,
            .column = std::move(column)
        });
        return *this;
    }

    Query& order_by(
        String column,
        Direction direction = Direction::asc
    ) {
        plan_.orders.push_back(Order{
            .column = std::move(column),
            .direction = direction
        });
        return *this;
    }

    Query& limit(std::size_t value) noexcept {
        plan_.limit = value;
        return *this;
    }

    Query& offset(std::size_t value) noexcept {
        plan_.offset = value;
        return *this;
    }

    Query& with(String relation) {
        bool known = false;

        model::for_each_relation<ModelType>([&](const auto& descriptor) {
            if (descriptor.name == relation) {
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

    Query& find(model::AttributeValue key) {
        return where_(
            String{ModelType::primary_key_name()},
            std::move(key)
        ).limit(1);
    }

    [[nodiscard]] CompiledQuery compile(
        database::Backend backend
    ) const {
        return orm::compile(plan_, backend);
    }

    [[nodiscard]] Collection<ModelType> get() const;
    [[nodiscard]] std::optional<ModelType> first() const;
    [[nodiscard]] ModelType first_or_fail() const;

private:
    void add_comparison(
        BooleanConnector connector,
        String column,
        Operator comparison,
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

    QueryPlan plan_;
};

} // namespace gungnir::orm

namespace gungnir {

template <typename Derived>
orm::Query<Derived> Model<Derived>::query() {
    return orm::Query<Derived>{};
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::where_(
    String column,
    model::AttributeValue value
) {
    return query().where_(std::move(column), std::move(value));
}

template <typename Derived>
orm::Query<Derived> Model<Derived>::with(String relation) {
    return query().with(std::move(relation));
}

} // namespace gungnir
