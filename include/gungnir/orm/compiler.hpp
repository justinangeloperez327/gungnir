#pragma once

#include <vector>

#include <gungnir/core/types.hpp>
#include <gungnir/database/backend.hpp>
#include <gungnir/model/value.hpp>
#include <gungnir/orm/plan.hpp>

namespace gungnir::orm {

enum class CompiledQueryKind {
    sql,
    mongodb
};

struct CompiledQuery {
    CompiledQueryKind kind{CompiledQueryKind::sql};
    String text;
    std::vector<model::AttributeValue> bindings;
};

[[nodiscard]] CompiledQuery compile(
    const QueryPlan& plan,
    database::Backend backend
);

} // namespace gungnir::orm
