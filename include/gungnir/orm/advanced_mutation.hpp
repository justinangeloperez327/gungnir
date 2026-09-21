#pragma once

#include <vector>

#include <gungnir/database/backend.hpp>
#include <gungnir/model/value.hpp>
#include <gungnir/orm/compiler.hpp>
#include <gungnir/orm/plan.hpp>

namespace gungnir::orm {

[[nodiscard]] CompiledQuery compile_update_where(
    const QueryPlan& plan,
    const model::AttributeMap& values,
    database::Backend backend
);

[[nodiscard]] CompiledQuery compile_delete_where(
    const QueryPlan& plan,
    database::Backend backend
);

[[nodiscard]] CompiledQuery compile_increment_where(
    const QueryPlan& plan,
    String column,
    Double amount,
    database::Backend backend
);

[[nodiscard]] CompiledQuery compile_insert_many(
    String table,
    const std::vector<model::AttributeMap>& rows,
    database::Backend backend
);

[[nodiscard]] CompiledQuery compile_upsert(
    String table,
    const std::vector<model::AttributeMap>& rows,
    const std::vector<String>& unique_by,
    const std::vector<String>& update_columns,
    database::Backend backend,
    bool ignore_conflicts = false
);

[[nodiscard]] CompiledQuery compile_soft_delete_where(
    const QueryPlan& plan,
    String column,
    bool restore,
    database::Backend backend
);

} // namespace gungnir::orm
