#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <gungnir/core/types.hpp>
#include <gungnir/model/value.hpp>

namespace gungnir::orm {

enum class Comparison {
    equal,
    not_equal,
    less_than,
    less_or_equal,
    greater_than,
    greater_or_equal,
    like
};

enum class BooleanConnector {
    and_,
    or_
};

enum class PredicateKind {
    comparison,
    column_comparison,
    in_list,
    not_in_list,
    between,
    not_between,
    is_null,
    is_not_null
};

enum class SortDirection {
    asc,
    desc
};

enum class JoinType {
    inner,
    left,
    right,
    cross
};

enum class LockMode {
    none,
    for_update,
    shared
};

enum class AggregateFunction {
    count,
    sum,
    average,
    minimum,
    maximum
};

struct Predicate {
    PredicateKind kind{PredicateKind::comparison};
    BooleanConnector connector{BooleanConnector::and_};
    String column;
    Comparison comparison{Comparison::equal};
    String other_column;
    std::vector<model::AttributeValue> values;
    bool automatic{false};
};

struct Order {
    String column;
    SortDirection direction{SortDirection::asc};
};

struct Join {
    JoinType type{JoinType::inner};
    String table;
    String first;
    Comparison comparison{Comparison::equal};
    String second;
};

struct Aggregate {
    AggregateFunction function{AggregateFunction::count};
    String column{"*"};
};

struct QueryPlan {
    String table;
    String connection{"default"};
    std::vector<String> columns;
    std::vector<Join> joins;
    std::vector<Predicate> predicates;
    std::vector<String> groups;
    std::vector<Predicate> having;
    std::vector<Order> orders;
    std::vector<String> eager_loads;
    std::optional<Aggregate> aggregate;
    std::optional<std::size_t> limit;
    std::optional<std::size_t> offset;
    bool distinct{false};
    LockMode lock{LockMode::none};
};

} // namespace gungnir::orm
