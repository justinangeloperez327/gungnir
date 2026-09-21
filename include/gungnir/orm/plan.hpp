#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <gungnir/core/types.hpp>
#include <gungnir/model/value.hpp>

namespace gungnir::orm {

enum class QueryType {
    select
};

enum class Operator {
    equal,
    not_equal,
    less,
    less_or_equal,
    greater,
    greater_or_equal,
    like
};

enum class BooleanConnector {
    and_,
    or_
};

enum class PredicateKind {
    comparison,
    in,
    not_in,
    null_,
    not_null
};

enum class Direction {
    asc,
    desc
};

struct Predicate {
    PredicateKind kind{PredicateKind::comparison};
    BooleanConnector connector{BooleanConnector::and_};
    String column;
    Operator comparison{Operator::equal};
    std::vector<model::AttributeValue> values;
};

struct Order {
    String column;
    Direction direction{Direction::asc};
};

struct QueryPlan {
    QueryType type{QueryType::select};
    String table;
    String connection{"default"};
    std::vector<String> columns;
    std::vector<Predicate> predicates;
    std::vector<Order> orders;
    std::vector<String> eager_loads;
    std::optional<std::size_t> limit;
    std::optional<std::size_t> offset;
};

} // namespace gungnir::orm
