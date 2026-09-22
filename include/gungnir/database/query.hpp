#pragma once

#include <utility>
#include <vector>

#include <gungnir/core/types.hpp>
#include <gungnir/model/value.hpp>

namespace gungnir::database {

struct Query {
    String statement;
    std::vector<model::AttributeValue> bindings;

    Query() = default;
    explicit Query(String sql) : statement(std::move(sql)) {}
    Query(String sql, std::vector<model::AttributeValue> values)
        : statement(std::move(sql)), bindings(std::move(values)) {}
};

} // namespace gungnir::database
