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
    in_list,
    not_in_list,
    is_null,
    is_not_null
};

enum class SortDirection {
    asc,
    desc
};

struct Predicate {
    PredicateKind kind{PredicateKind::comparison};
    BooleanConnector connector{BooleanConnector::and_};
    String column;
    Comparison comparison{Comparison::equal};
    std::vector<model::AttributeValue> values;
};

struct Order {
    String column;
    SortDirection direction{SortDirection::asc};
};

struct QueryPlan {
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
