#pragma once

#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

#include <gungnir/core/types.hpp>

namespace gungnir::migration {

enum class ColumnType {
    id,
    integer,
    big_integer,
    string,
    text,
    boolean,
    decimal,
    date_time,
    timestamp
};

using DefaultValue = std::variant<std::monostate, Boolean, Int64, Double, String>;

struct ForeignReference {
    String table;
    String column{"id"};
    Boolean cascade_delete{false};
};

struct ColumnDefinition {
    String name;
    ColumnType type;
    std::size_t length{0};
    Boolean nullable_value{false};
    Boolean unique_value{false};
    Boolean index_value{false};
    Boolean primary_value{false};
    Boolean auto_increment_value{false};
    DefaultValue default_value_data{};
    std::optional<ForeignReference> reference;

    ColumnDefinition& nullable(Boolean value = true) noexcept;
    ColumnDefinition& unique(Boolean value = true) noexcept;
    ColumnDefinition& index(Boolean value = true) noexcept;
    ColumnDefinition& primary(Boolean value = true) noexcept;
    ColumnDefinition& auto_increment(Boolean value = true) noexcept;

    ColumnDefinition& default_value(String value);
    ColumnDefinition& default_value(const char* value);
    ColumnDefinition& default_value(Integer value) noexcept;
    ColumnDefinition& default_value(Int64 value) noexcept;
    ColumnDefinition& default_value(Double value) noexcept;
    ColumnDefinition& default_value(Boolean value) noexcept;

    ColumnDefinition& references(String column);
    ColumnDefinition& on(String table);
    ColumnDefinition& cascade_on_delete(Boolean value = true);
};

enum class TableOperationType {
    create,
    alter,
    drop,
    drop_if_exists
};

struct TableOperation {
    TableOperationType type;
    String name;
    std::vector<ColumnDefinition> columns;
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
