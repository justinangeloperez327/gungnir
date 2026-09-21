#include <gungnir/migration/definition.hpp>

#include <utility>

namespace gungnir::migration {

ColumnDefinition& ColumnDefinition::nullable(Boolean value) noexcept {
    nullable_value = value;
    return *this;
}

ColumnDefinition& ColumnDefinition::unique(Boolean value) noexcept {
    unique_value = value;
    return *this;
}

ColumnDefinition& ColumnDefinition::index(Boolean value) noexcept {
    index_value = value;
    return *this;
}

ColumnDefinition& ColumnDefinition::primary(Boolean value) noexcept {
    primary_value = value;
    return *this;
}

ColumnDefinition& ColumnDefinition::auto_increment(Boolean value) noexcept {
    auto_increment_value = value;
    return *this;
}

ColumnDefinition& ColumnDefinition::default_value(String value) {
    default_value_data = std::move(value);
    return *this;
}

ColumnDefinition& ColumnDefinition::default_value(const char* value) {
    default_value_data = String{value};
    return *this;
}

ColumnDefinition& ColumnDefinition::default_value(Integer value) noexcept {
    default_value_data = static_cast<Int64>(value);
    return *this;
}

ColumnDefinition& ColumnDefinition::default_value(Int64 value) noexcept {
    default_value_data = value;
    return *this;
}

ColumnDefinition& ColumnDefinition::default_value(Double value) noexcept {
    default_value_data = value;
    return *this;
}

ColumnDefinition& ColumnDefinition::default_value(Boolean value) noexcept {
    default_value_data = value;
    return *this;
}

ColumnDefinition& ColumnDefinition::references(String column) {
    if (!reference) {
        reference.emplace();
    }
    reference->column = std::move(column);
    return *this;
}

ColumnDefinition& ColumnDefinition::on(String table) {
    if (!reference) {
        reference.emplace();
    }
    reference->table = std::move(table);
    return *this;
}

ColumnDefinition& ColumnDefinition::cascade_on_delete(Boolean value) {
    if (!reference) {
        reference.emplace();
    }
    reference->cascade_delete = value;
    return *this;
}

void Plan::add(TableOperation operation) {
    operations_.push_back(std::move(operation));
}

const std::vector<TableOperation>& Plan::operations() const noexcept {
    return operations_;
}

Boolean Plan::empty() const noexcept {
    return operations_.empty();
}

} // namespace gungnir::migration
