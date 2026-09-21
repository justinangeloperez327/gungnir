#include <gungnir/migration/table.hpp>

#include "context.hpp"

#include <stdexcept>
#include <utility>

namespace gungnir::migration {

namespace {

Plan& active_plan() {
    auto* plan = detail::current_plan();
    if (plan == nullptr) {
        throw std::logic_error(
            "Table migration operations must run inside Migration::plan_up() or plan_down()"
        );
    }
    return *plan;
}

void define_table(
    TableOperationType type,
    String name,
    Table::Definition definition
) {
    TableOperation operation{
        .type = type,
        .name = std::move(name),
        .columns = {}
    };

    Column column{operation.columns};
    definition(column);
    active_plan().add(std::move(operation));
}

} // namespace

void Table::create(String name, Definition definition) {
    define_table(TableOperationType::create, std::move(name), std::move(definition));
}

void Table::alter(String name, Definition definition) {
    define_table(TableOperationType::alter, std::move(name), std::move(definition));
}

void Table::drop(String name) {
    active_plan().add(TableOperation{
        .type = TableOperationType::drop,
        .name = std::move(name),
        .columns = {}
    });
}

void Table::drop_if_exists(String name) {
    active_plan().add(TableOperation{
        .type = TableOperationType::drop_if_exists,
        .name = std::move(name),
        .columns = {}
    });
}

} // namespace gungnir::migration
