#pragma once

#include <string_view>

#include <gungnir/database/backend.hpp>
#include <gungnir/model/value.hpp>
#include <gungnir/orm/compiler.hpp>

namespace gungnir::orm {

[[nodiscard]] CompiledQuery compile_insert(
    std::string_view table,
    const model::AttributeMap& attributes,
    database::Backend backend
);

[[nodiscard]] CompiledQuery compile_update(
    std::string_view table,
    const model::AttributeMap& attributes,
    std::string_view key,
    model::AttributeValue key_value,
    database::Backend backend
);

[[nodiscard]] CompiledQuery compile_delete(
    std::string_view table,
    std::string_view key,
    model::AttributeValue key_value,
    database::Backend backend
);

} // namespace gungnir::orm
