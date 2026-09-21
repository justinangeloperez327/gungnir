#pragma once

#include <cmath>
#include <stdexcept>
#include <utility>

#include <gungnir/database/connection.hpp>
#include <gungnir/database/runtime.hpp>
#include <gungnir/orm/advanced_mutation.hpp>
#include <gungnir/orm/mutation.hpp>
#include <gungnir/orm/query.hpp>

namespace gungnir::orm {

namespace detail {

inline Double numeric_value(
    const model::AttributeValue& value
) {
    if (const auto* number = std::get_if<Double>(&value)) {
        return *number;
    }
    if (const auto* number = std::get_if<Int64>(&value)) {
        return static_cast<Double>(*number);
    }
    if (const auto* number = std::get_if<UInt64>(&value)) {
        return static_cast<Double>(*number);
    }

    throw std::invalid_argument(
        "ORM aggregate result is not numeric"
    );
}

template <typename ModelType>
database::Result execute_plan(const QueryPlan& plan) {
    auto connection = database::runtime::connection(plan.connection);
    const auto compiled = orm::compile(plan, connection->backend());
    return connection->execute(compiled.text, compiled.bindings);
}

template <typename ModelType>
std::optional<model::AttributeValue> aggregate(
    const Query<ModelType>& query,
    AggregateFunction function,
    String column
) {
    auto plan = query.plan();
    plan.columns.clear();
    plan.groups.clear();
    plan.having.clear();
    plan.orders.clear();
    plan.eager_loads.clear();
    plan.limit.reset();
    plan.offset.reset();
    plan.distinct = false;
    plan.aggregate = Aggregate{
        .function = function,
        .column = std::move(column)
    };

    const auto result = execute_plan<ModelType>(plan);
    if (result.rows.empty()) {
        return std::nullopt;
    }

    const auto found = result.rows.front().find("aggregate");
    if (found == result.rows.front().end()) {
        return std::nullopt;
    }

    return found->second;
}

} // namespace detail

template <typename ModelType>
std::vector<model::AttributeValue> Query<ModelType>::pluck(
    String column
) const {
    auto plan = plan_;
    plan.columns = {column};
    plan.eager_loads.clear();

    const auto result = detail::execute_plan<ModelType>(plan);
    std::vector<model::AttributeValue> values;
    values.reserve(result.rows.size());

    for (const auto& row : result.rows) {
        const auto found = row.find(column);
        if (found != row.end()) {
            values.push_back(found->second);
        }
    }

    return values;
}

template <typename ModelType>
std::optional<model::AttributeValue> Query<ModelType>::value(
    String column
) const {
    auto plan = plan_;
    plan.columns = {column};
    plan.eager_loads.clear();
    plan.limit = 1;

    const auto result = detail::execute_plan<ModelType>(plan);
    if (result.rows.empty()) {
        return std::nullopt;
    }

    const auto found = result.rows.front().find(column);
    if (found == result.rows.front().end()) {
        return std::nullopt;
    }

    return found->second;
}

template <typename ModelType>
std::size_t Query<ModelType>::count() const {
    const auto result = detail::aggregate(
        *this,
        AggregateFunction::count,
        "*"
    );

    if (!result) {
        return 0;
    }

    return static_cast<std::size_t>(
        detail::numeric_value(*result)
    );
}

template <typename ModelType>
bool Query<ModelType>::exists() const {
    auto plan = plan_;
    plan.columns = {String{ModelType::primary_key_name()}};
    plan.eager_loads.clear();
    plan.limit = 1;
    plan.offset.reset();

    const auto result = detail::execute_plan<ModelType>(plan);
    return !result.rows.empty();
}

template <typename ModelType>
Double Query<ModelType>::sum(String column) const {
    const auto result = detail::aggregate(
        *this,
        AggregateFunction::sum,
        std::move(column)
    );

    return result ? detail::numeric_value(*result) : 0.0;
}

template <typename ModelType>
Double Query<ModelType>::average(String column) const {
    const auto result = detail::aggregate(
        *this,
        AggregateFunction::average,
        std::move(column)
    );

    return result ? detail::numeric_value(*result) : 0.0;
}

template <typename ModelType>
std::optional<model::AttributeValue> Query<ModelType>::min(
    String column
) const {
    return detail::aggregate(
        *this,
        AggregateFunction::minimum,
        std::move(column)
    );
}

template <typename ModelType>
std::optional<model::AttributeValue> Query<ModelType>::max(
    String column
) const {
    return detail::aggregate(
        *this,
        AggregateFunction::maximum,
        std::move(column)
    );
}

template <typename ModelType>
Page<ModelType> Query<ModelType>::paginate(
    std::size_t page,
    std::size_t per_page
) const {
    if (page == 0 || per_page == 0) {
        throw std::invalid_argument(
            "ORM pagination page and per_page must be greater than zero"
        );
    }

    if (plan_.distinct || !plan_.groups.empty() || !plan_.having.empty()) {
        throw std::logic_error(
            "ORM paginate() currently requires an ungrouped, non-distinct model query"
        );
    }

    const auto total = count();
    auto query = *this;
    query.plan_.limit = per_page;
    query.plan_.offset = (page - 1) * per_page;

    const auto last_page = std::max<std::size_t>(
        1,
        static_cast<std::size_t>(
            std::ceil(
                static_cast<Double>(total) /
                static_cast<Double>(per_page)
            )
        )
    );

    return Page<ModelType>{
        .data = query.get(),
        .current_page = page,
        .per_page = per_page,
        .total = total,
        .last_page = last_page
    };
}

template <typename ModelType>
std::size_t Query<ModelType>::update(
    const model::AttributeMap& values
) const {
    if (values.empty()) {
        return 0;
    }

    auto connection = database::runtime::connection(plan_.connection);
    const auto compiled = compile_update_where(
        plan_,
        values,
        connection->backend()
    );

    return connection->execute(
        compiled.text,
        compiled.bindings
    ).affected_rows;
}

template <typename ModelType>
std::size_t Query<ModelType>::remove() const {
    if constexpr (ModelType::uses_soft_deletes()) {
        auto connection = database::runtime::connection(plan_.connection);
        const auto compiled = compile_soft_delete_where(
            plan_,
            String{ModelType::deleted_at_column()},
            false,
            connection->backend()
        );

        return connection->execute(
            compiled.text,
            compiled.bindings
        ).affected_rows;
    }

    return force_remove();
}

template <typename ModelType>
std::size_t Query<ModelType>::force_remove() const {
    auto connection = database::runtime::connection(plan_.connection);
    const auto compiled = compile_delete_where(
        plan_,
        connection->backend()
    );

    return connection->execute(
        compiled.text,
        compiled.bindings
    ).affected_rows;
}

template <typename ModelType>
std::size_t Query<ModelType>::restore() const {
    if constexpr (!ModelType::uses_soft_deletes()) {
        return 0;
    }

    auto query = *this;
    query.erase_soft_delete_scope();

    auto connection = database::runtime::connection(query.plan_.connection);
    const auto compiled = compile_soft_delete_where(
        query.plan_,
        String{ModelType::deleted_at_column()},
        true,
        connection->backend()
    );

    return connection->execute(
        compiled.text,
        compiled.bindings
    ).affected_rows;
}

template <typename ModelType>
std::size_t Query<ModelType>::increment(
    String column,
    Double amount
) const {
    auto connection = database::runtime::connection(plan_.connection);
    const auto compiled = compile_increment_where(
        plan_,
        std::move(column),
        amount,
        connection->backend()
    );

    return connection->execute(
        compiled.text,
        compiled.bindings
    ).affected_rows;
}

template <typename ModelType>
std::size_t Query<ModelType>::decrement(
    String column,
    Double amount
) const {
    return increment(std::move(column), -amount);
}

template <typename ModelType>
bool Query<ModelType>::insert(
    const model::AttributeMap& values
) const {
    auto connection = database::runtime::connection(plan_.connection);
    const auto compiled = compile_insert(
        plan_.table,
        values,
        connection->backend()
    );

    return connection->execute(
        compiled.text,
        compiled.bindings
    ).affected_rows > 0;
}

template <typename ModelType>
std::size_t Query<ModelType>::insert_many(
    const std::vector<model::AttributeMap>& rows
) const {
    if (rows.empty()) {
        return 0;
    }

    auto connection = database::runtime::connection(plan_.connection);
    const auto compiled = compile_insert_many(
        plan_.table,
        rows,
        connection->backend()
    );

    return connection->execute(
        compiled.text,
        compiled.bindings
    ).affected_rows;
}

template <typename ModelType>
std::size_t Query<ModelType>::upsert(
    const std::vector<model::AttributeMap>& rows,
    std::vector<String> unique_by,
    std::vector<String> update_columns
) const {
    if (rows.empty()) {
        return 0;
    }

    auto connection = database::runtime::connection(plan_.connection);
    const auto compiled = compile_upsert(
        plan_.table,
        rows,
        unique_by,
        update_columns,
        connection->backend(),
        false
    );

    return connection->execute(
        compiled.text,
        compiled.bindings
    ).affected_rows;
}

template <typename ModelType>
std::size_t Query<ModelType>::insert_or_ignore(
    const std::vector<model::AttributeMap>& rows,
    std::vector<String> unique_by
) const {
    if (rows.empty()) {
        return 0;
    }

    auto connection = database::runtime::connection(plan_.connection);
    const auto compiled = compile_upsert(
        plan_.table,
        rows,
        unique_by,
        {},
        connection->backend(),
        true
    );

    return connection->execute(
        compiled.text,
        compiled.bindings
    ).affected_rows;
}

} // namespace gungnir::orm
