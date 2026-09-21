#include <gungnir/migration/definition.hpp>

#include <utility>

namespace gungnir::migration {

namespace {

ForeignReference& ensure_reference(ColumnDefinition& definition) {
    if (!definition.reference) {
        definition.reference.emplace();
    }
    return *definition.reference;
}

} // namespace

ColumnDefinition& ColumnDefinition::nullable(Boolean value) noexcept {
    nullable_value = value;
    return *this;
}

ColumnDefinition& ColumnDefinition::unsigned_(Boolean value) noexcept {
    unsigned_value = value;
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

ColumnDefinition& ColumnDefinition::change(Boolean value) noexcept {
    change_value = value;
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

ColumnDefinition& ColumnDefinition::default_value(UInt64 value) noexcept {
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

ColumnDefinition& ColumnDefinition::default_null() noexcept {
    default_value_data = nullptr;
    return *this;
}

ColumnDefinition& ColumnDefinition::default_expression(String expression) {
    default_value_data = SqlExpression{std::move(expression)};
    return *this;
}

ColumnDefinition& ColumnDefinition::use_current() {
    return default_expression("CURRENT_TIMESTAMP");
}

ColumnDefinition& ColumnDefinition::use_current_on_update(Boolean value) noexcept {
    use_current_on_update_value = value;
    return *this;
}

ColumnDefinition& ColumnDefinition::comment(String value) {
    comment_value = std::move(value);
    return *this;
}

ColumnDefinition& ColumnDefinition::collation(String value) {
    collation_value = std::move(value);
    return *this;
}

ColumnDefinition& ColumnDefinition::references(String column) {
    ensure_reference(*this).column = std::move(column);
    return *this;
}

ColumnDefinition& ColumnDefinition::on(String table) {
    ensure_reference(*this).table = std::move(table);
    return *this;
}

ColumnDefinition& ColumnDefinition::constrained(String table, String column) {
    auto& foreign = ensure_reference(*this);
    foreign.table = std::move(table);
    foreign.column = std::move(column);
    return *this;
}

ColumnDefinition& ColumnDefinition::on_delete(ReferentialAction action) {
    ensure_reference(*this).on_delete = action;
    return *this;
}

ColumnDefinition& ColumnDefinition::on_update(ReferentialAction action) {
    ensure_reference(*this).on_update = action;
    return *this;
}

ColumnDefinition& ColumnDefinition::cascade_on_delete() {
    return on_delete(ReferentialAction::cascade);
}

ColumnDefinition& ColumnDefinition::restrict_on_delete() {
    return on_delete(ReferentialAction::restrict_);
}

ColumnDefinition& ColumnDefinition::null_on_delete() {
    return on_delete(ReferentialAction::set_null);
}

ColumnDefinition& ColumnDefinition::cascade_on_update() {
    return on_update(ReferentialAction::cascade);
}

ColumnDefinition& ColumnDefinition::restrict_on_update() {
    return on_update(ReferentialAction::restrict_);
}

ColumnDefinition& ColumnDefinition::null_on_update() {
    return on_update(ReferentialAction::set_null);
}

ForeignKeyDefinition& ForeignKeyDefinition::references(String column) {
    referenced_columns = {std::move(column)};
    return *this;
}

ForeignKeyDefinition& ForeignKeyDefinition::references(
    std::initializer_list<String> referenced
) {
    referenced_columns.assign(referenced.begin(), referenced.end());
    return *this;
}

ForeignKeyDefinition& ForeignKeyDefinition::on(String table_name) {
    table = std::move(table_name);
    return *this;
}

ForeignKeyDefinition& ForeignKeyDefinition::on_delete(
    ReferentialAction action
) noexcept {
    on_delete_action = action;
    return *this;
}

ForeignKeyDefinition& ForeignKeyDefinition::on_update(
    ReferentialAction action
) noexcept {
    on_update_action = action;
    return *this;
}

ForeignKeyDefinition& ForeignKeyDefinition::cascade_on_delete() noexcept {
    return on_delete(ReferentialAction::cascade);
}

ForeignKeyDefinition& ForeignKeyDefinition::restrict_on_delete() noexcept {
    return on_delete(ReferentialAction::restrict_);
}

ForeignKeyDefinition& ForeignKeyDefinition::null_on_delete() noexcept {
    return on_delete(ReferentialAction::set_null);
}

ForeignKeyDefinition& ForeignKeyDefinition::cascade_on_update() noexcept {
    return on_update(ReferentialAction::cascade);
}

ForeignKeyDefinition& ForeignKeyDefinition::restrict_on_update() noexcept {
    return on_update(ReferentialAction::restrict_);
}

ForeignKeyDefinition& ForeignKeyDefinition::null_on_update() noexcept {
    return on_update(ReferentialAction::set_null);
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
