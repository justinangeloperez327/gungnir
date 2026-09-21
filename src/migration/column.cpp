#include <gungnir/migration/column.hpp>

#include <utility>

namespace gungnir::migration {

Column::Column(std::vector<ColumnDefinition>& columns) noexcept
    : columns_(columns) {}

ColumnDefinition& Column::add(
    String name,
    ColumnType type,
    std::size_t length
) {
    columns_.push_back(ColumnDefinition{
        .name = std::move(name),
        .type = type,
        .length = length,
        .nullable_value = false,
        .unique_value = false,
        .index_value = false,
        .primary_value = false,
        .auto_increment_value = false,
        .default_value_data = {},
        .reference = std::nullopt
    });
    return columns_.back();
}

ColumnDefinition& Column::id(String name) {
    return add(std::move(name), ColumnType::id)
        .primary()
        .auto_increment();
}

ColumnDefinition& Column::integer(String name) {
    return add(std::move(name), ColumnType::integer);
}

ColumnDefinition& Column::big_integer(String name) {
    return add(std::move(name), ColumnType::big_integer);
}

ColumnDefinition& Column::string(String name, std::size_t length) {
    return add(std::move(name), ColumnType::string, length);
}

ColumnDefinition& Column::text(String name) {
    return add(std::move(name), ColumnType::text);
}

ColumnDefinition& Column::boolean(String name) {
    return add(std::move(name), ColumnType::boolean);
}

ColumnDefinition& Column::decimal(String name) {
    return add(std::move(name), ColumnType::decimal);
}

ColumnDefinition& Column::date_time(String name) {
    return add(std::move(name), ColumnType::date_time);
}

ColumnDefinition& Column::timestamp(String name) {
    return add(std::move(name), ColumnType::timestamp);
}

ColumnDefinition& Column::foreign_id(String name) {
    return add(std::move(name), ColumnType::big_integer).index();
}

void Column::timestamps() {
    timestamp("created_at").nullable();
    timestamp("updated_at").nullable();
}

} // namespace gungnir::migration
