#pragma once

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <variant>
#include <vector>

#include <gungnir/core/types.hpp>

namespace gungnir::migration {

enum class ColumnType {
    id,
    tiny_integer,
    small_integer,
    medium_integer,
    integer,
    big_integer,
    floating,
    double_precision,
    decimal,
    boolean,
    fixed_string,
    string,
    text,
    medium_text,
    long_text,
    binary,
    json,
    uuid,
    date,
    time,
    date_time,
    timestamp,
    timestamp_tz,
    enumeration
};

enum class ReferentialAction {
    no_action,
    restrict_,
    cascade,
    set_null,
    set_default
};

struct SqlExpression {
    String value;
};

using DefaultValue = std::variant<
    std::monostate,
    std::nullptr_t,
    Boolean,
    Int64,
    UInt64,
    Double,
    String,
    SqlExpression
>;

struct ForeignReference {
    String table;
    String column{"id"};
    ReferentialAction on_delete{ReferentialAction::no_action};
    ReferentialAction on_update{ReferentialAction::no_action};
};

struct ColumnDefinition {
    String name;
    ColumnType type;
    std::size_t length{0};
    std::size_t precision{0};
    std::size_t scale{0};
    std::vector<String> allowed_values;

    Boolean nullable_value{false};
    Boolean unsigned_value{false};
    Boolean unique_value{false};
    Boolean index_value{false};
    Boolean primary_value{false};
    Boolean auto_increment_value{false};
    Boolean change_value{false};
    Boolean use_current_on_update_value{false};

    DefaultValue default_value_data{};
    String comment_value;
    String collation_value;
    std::optional<ForeignReference> reference;

    ColumnDefinition& nullable(Boolean value = true) noexcept;
    ColumnDefinition& unsigned_(Boolean value = true) noexcept;
    ColumnDefinition& unique(Boolean value = true) noexcept;
    ColumnDefinition& index(Boolean value = true) noexcept;
    ColumnDefinition& primary(Boolean value = true) noexcept;
    ColumnDefinition& auto_increment(Boolean value = true) noexcept;
    ColumnDefinition& change(Boolean value = true) noexcept;

    ColumnDefinition& default_value(String value);
    ColumnDefinition& default_value(const char* value);
    ColumnDefinition& default_value(Integer value) noexcept;
    ColumnDefinition& default_value(Int64 value) noexcept;
    ColumnDefinition& default_value(UInt64 value) noexcept;
    ColumnDefinition& default_value(Double value) noexcept;
    ColumnDefinition& default_value(Boolean value) noexcept;
    ColumnDefinition& default_null() noexcept;
    ColumnDefinition& default_expression(String expression);
    ColumnDefinition& use_current();
    ColumnDefinition& use_current_on_update(Boolean value = true) noexcept;

    ColumnDefinition& comment(String value);
    ColumnDefinition& collation(String value);

    ColumnDefinition& references(String column);
    ColumnDefinition& on(String table);
    ColumnDefinition& constrained(String table, String column = "id");
    ColumnDefinition& on_delete(ReferentialAction action);
    ColumnDefinition& on_update(ReferentialAction action);
    ColumnDefinition& cascade_on_delete();
    ColumnDefinition& restrict_on_delete();
    ColumnDefinition& null_on_delete();
    ColumnDefinition& cascade_on_update();
    ColumnDefinition& restrict_on_update();
    ColumnDefinition& null_on_update();
};

enum class IndexType {
    primary,
    unique,
    index
};

struct IndexDefinition {
    IndexType type{IndexType::index};
    std::vector<String> columns;
    String name;
};

struct ForeignKeyDefinition {
    std::vector<String> columns;
    String table;
    std::vector<String> referenced_columns;
    String name;
    ReferentialAction on_delete_action{ReferentialAction::no_action};
    ReferentialAction on_update_action{ReferentialAction::no_action};

    ForeignKeyDefinition& references(String column);
    ForeignKeyDefinition& references(std::initializer_list<String> columns);
    ForeignKeyDefinition& on(String table_name);
    ForeignKeyDefinition& on_delete(ReferentialAction action) noexcept;
    ForeignKeyDefinition& on_update(ReferentialAction action) noexcept;
    ForeignKeyDefinition& cascade_on_delete() noexcept;
    ForeignKeyDefinition& restrict_on_delete() noexcept;
    ForeignKeyDefinition& null_on_delete() noexcept;
    ForeignKeyDefinition& cascade_on_update() noexcept;
    ForeignKeyDefinition& restrict_on_update() noexcept;
    ForeignKeyDefinition& null_on_update() noexcept;
};

enum class AlterCommandType {
    drop_column,
    rename_column,
    drop_primary,
    drop_unique,
    drop_index,
    rename_index,
    drop_foreign
};

struct AlterCommand {
    AlterCommandType type;
    std::vector<String> columns;
    String name;
    String new_name;
};

enum class TableOperationType {
    create,
    alter,
    rename,
    drop,
    drop_if_exists
};

struct TableOperation {
    TableOperationType type;
    String name;
    String new_name;
    std::vector<ColumnDefinition> columns;
    std::vector<IndexDefinition> indexes;
    std::vector<ForeignKeyDefinition> foreign_keys;
    std::vector<AlterCommand> commands;
};

class Plan {
public:
    void add(TableOperation operation);

    [[nodiscard]] const std::vector<TableOperation>& operations() const noexcept;
    [[nodiscard]] Boolean empty() const noexcept;

private:
    std::vector<TableOperation> operations_;
};

} // namespace gungnir::migration
