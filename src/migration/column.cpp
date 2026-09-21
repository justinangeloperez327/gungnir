#include <gungnir/migration/column.hpp>

#include <utility>

namespace gungnir::migration {

Column::Column(TableOperation& operation) noexcept
    : operation_(operation) {}

ColumnDefinition& Column::add(
    String name,
    ColumnType type,
    std::size_t length,
    std::size_t precision,
    std::size_t scale
) {
    operation_.columns.push_back(ColumnDefinition{
        .name = std::move(name),
        .type = type,
        .length = length,
        .precision = precision,
        .scale = scale,
        .allowed_values = {},
        .nullable_value = false,
        .unsigned_value = false,
        .unique_value = false,
        .index_value = false,
        .primary_value = false,
        .auto_increment_value = false,
        .change_value = false,
        .use_current_on_update_value = false,
        .default_value_data = {},
        .comment_value = {},
        .collation_value = {},
        .reference = std::nullopt
    });
    return operation_.columns.back();
}

ColumnDefinition& Column::id(String name) {
    return add(std::move(name), ColumnType::id)
        .primary()
        .auto_increment();
}

ColumnDefinition& Column::tiny_integer(String name) {
    return add(std::move(name), ColumnType::tiny_integer);
}

ColumnDefinition& Column::small_integer(String name) {
    return add(std::move(name), ColumnType::small_integer);
}

ColumnDefinition& Column::medium_integer(String name) {
    return add(std::move(name), ColumnType::medium_integer);
}

ColumnDefinition& Column::integer(String name) {
    return add(std::move(name), ColumnType::integer);
}

ColumnDefinition& Column::big_integer(String name) {
    return add(std::move(name), ColumnType::big_integer);
}

ColumnDefinition& Column::floating(String name) {
    return add(std::move(name), ColumnType::floating);
}

ColumnDefinition& Column::double_precision(String name) {
    return add(std::move(name), ColumnType::double_precision);
}

ColumnDefinition& Column::decimal(
    String name,
    std::size_t precision,
    std::size_t scale
) {
    return add(
        std::move(name),
        ColumnType::decimal,
        0,
        precision,
        scale
    );
}

ColumnDefinition& Column::boolean(String name) {
    return add(std::move(name), ColumnType::boolean);
}

ColumnDefinition& Column::fixed_string(String name, std::size_t length) {
    return add(std::move(name), ColumnType::fixed_string, length);
}

ColumnDefinition& Column::string(String name, std::size_t length) {
    return add(std::move(name), ColumnType::string, length);
}

ColumnDefinition& Column::text(String name) {
    return add(std::move(name), ColumnType::text);
}

ColumnDefinition& Column::medium_text(String name) {
    return add(std::move(name), ColumnType::medium_text);
}

ColumnDefinition& Column::long_text(String name) {
    return add(std::move(name), ColumnType::long_text);
}

ColumnDefinition& Column::binary(String name) {
    return add(std::move(name), ColumnType::binary);
}

ColumnDefinition& Column::json(String name) {
    return add(std::move(name), ColumnType::json);
}

ColumnDefinition& Column::uuid(String name) {
    return add(std::move(name), ColumnType::uuid);
}

ColumnDefinition& Column::date(String name) {
    return add(std::move(name), ColumnType::date);
}

ColumnDefinition& Column::time(String name) {
    return add(std::move(name), ColumnType::time);
}

ColumnDefinition& Column::date_time(String name) {
    return add(std::move(name), ColumnType::date_time);
}

ColumnDefinition& Column::timestamp(String name) {
    return add(std::move(name), ColumnType::timestamp);
}

ColumnDefinition& Column::timestamp_tz(String name) {
    return add(std::move(name), ColumnType::timestamp_tz);
}

ColumnDefinition& Column::enumeration(
    String name,
    std::initializer_list<String> values
) {
    auto& definition = add(std::move(name), ColumnType::enumeration);
    definition.allowed_values.assign(values.begin(), values.end());
    return definition;
}

ColumnDefinition& Column::foreign_id(String name) {
    return add(std::move(name), ColumnType::big_integer)
        .unsigned_()
        .index();
}

void Column::timestamps() {
    timestamp("created_at").nullable();
    timestamp("updated_at").nullable();
}

void Column::timestamps_tz() {
    timestamp_tz("created_at").nullable();
    timestamp_tz("updated_at").nullable();
}

ColumnDefinition& Column::soft_deletes(String name) {
    return timestamp(std::move(name)).nullable();
}

ColumnDefinition& Column::soft_deletes_tz(String name) {
    return timestamp_tz(std::move(name)).nullable();
}

IndexDefinition& Column::add_index(
    IndexType type,
    std::initializer_list<String> columns,
    String name
) {
    operation_.indexes.push_back(IndexDefinition{
        .type = type,
        .columns = std::vector<String>{columns},
        .name = std::move(name)
    });
    return operation_.indexes.back();
}

IndexDefinition& Column::primary(
    std::initializer_list<String> columns,
    String name
) {
    return add_index(IndexType::primary, columns, std::move(name));
}

IndexDefinition& Column::unique(
    std::initializer_list<String> columns,
    String name
) {
    return add_index(IndexType::unique, columns, std::move(name));
}

IndexDefinition& Column::index(
    std::initializer_list<String> columns,
    String name
) {
    return add_index(IndexType::index, columns, std::move(name));
}

ForeignKeyDefinition& Column::foreign(String column, String name) {
    return foreign({std::move(column)}, std::move(name));
}

ForeignKeyDefinition& Column::foreign(
    std::initializer_list<String> columns,
    String name
) {
    operation_.foreign_keys.push_back(ForeignKeyDefinition{
        .columns = std::vector<String>{columns},
        .table = {},
        .referenced_columns = {},
        .name = std::move(name),
        .on_delete_action = ReferentialAction::no_action,
        .on_update_action = ReferentialAction::no_action
    });
    return operation_.foreign_keys.back();
}

void Column::add_command(
    AlterCommandType type,
    std::vector<String> columns,
    String name,
    String new_name
) {
    operation_.commands.push_back(AlterCommand{
        .type = type,
        .columns = std::move(columns),
        .name = std::move(name),
        .new_name = std::move(new_name)
    });
}

void Column::drop(String name) {
    add_command(
        AlterCommandType::drop_column,
        {std::move(name)}
    );
}

void Column::drop(std::initializer_list<String> names) {
    add_command(
        AlterCommandType::drop_column,
        std::vector<String>{names}
    );
}

void Column::rename(String from, String to) {
    add_command(
        AlterCommandType::rename_column,
        {},
        std::move(from),
        std::move(to)
    );
}

void Column::drop_primary(String name) {
    add_command(
        AlterCommandType::drop_primary,
        {},
        std::move(name)
    );
}

void Column::drop_unique(String name) {
    add_command(
        AlterCommandType::drop_unique,
        {},
        std::move(name)
    );
}

void Column::drop_index(String name) {
    add_command(
        AlterCommandType::drop_index,
        {},
        std::move(name)
    );
}

void Column::rename_index(String from, String to) {
    add_command(
        AlterCommandType::rename_index,
        {},
        std::move(from),
        std::move(to)
    );
}

void Column::drop_foreign(String name) {
    add_command(
        AlterCommandType::drop_foreign,
        {},
        std::move(name)
    );
}

void Column::drop_foreign(std::initializer_list<String> columns) {
    add_command(
        AlterCommandType::drop_foreign,
        std::vector<String>{columns}
    );
}

void Column::drop_timestamps() {
    drop({"created_at", "updated_at"});
}

void Column::drop_soft_deletes(String name) {
    drop(std::move(name));
}

} // namespace gungnir::migration
