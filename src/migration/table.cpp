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

TableOperation make_operation(TableOperationType type, String name) {
    return TableOperation{
        .type = type,
        .name = std::move(name),
        .new_name = {},
        .columns = {},
        .indexes = {},
        .foreign_keys = {},
        .commands = {}
    };
}

void define_table(
    TableOperationType type,
    String name,
    Table::Definition definition
) {
    auto operation = make_operation(type, std::move(name));

    Column column{operation};
    definition(column);
    active_plan().add(std::move(operation));
}

} // namespace

void Table::create(String name, Definition definition) {
    define_table(
        TableOperationType::create,
        std::move(name),
        std::move(definition)
    );
}

void Table::alter(String name, Definition definition) {
    define_table(
        TableOperationType::alter,
        std::move(name),
        std::move(definition)
    );
}

void Table::rename(String from, String to) {
    auto operation = make_operation(
        TableOperationType::rename,
        std::move(from)
    );
    operation.new_name = std::move(to);
    active_plan().add(std::move(operation));
}

void Table::drop(String name) {
    active_plan().add(
        make_operation(TableOperationType::drop, std::move(name))
    );
}

void Table::drop_if_exists(String name) {
    active_plan().add(
        make_operation(TableOperationType::drop_if_exists, std::move(name))
    );
}

} // namespace gungnir::migration
