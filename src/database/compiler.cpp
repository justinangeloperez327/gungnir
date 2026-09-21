#include <gungnir/database/compiler.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace gungnir::database {

namespace {

using migration::AlterCommand;
using migration::AlterCommandType;
using migration::ColumnDefinition;
using migration::ColumnType;
using migration::DefaultValue;
using migration::ForeignKeyDefinition;
using migration::IndexDefinition;
using migration::IndexType;
using migration::ReferentialAction;
using migration::SqlExpression;
using migration::TableOperation;
using migration::TableOperationType;

String escape_sql_string(const String& value) {
    String escaped;
    escaped.reserve(value.size());

    for (const char ch : value) {
        escaped.push_back(ch);
        if (ch == '\'') {
            escaped.push_back('\'');
        }
    }

    return escaped;
}

String quote_identifier(Backend backend, const String& value) {
    if (backend == Backend::mysql) {
        String escaped;
        for (const char ch : value) {
            escaped.push_back(ch);
            if (ch == '`') {
                escaped.push_back('`');
            }
        }
        return "`" + escaped + "`";
    }

    if (backend == Backend::mssql) {
        String escaped;
        for (const char ch : value) {
            escaped.push_back(ch);
            if (ch == ']') {
                escaped.push_back(']');
            }
        }
        return "[" + escaped + "]";
    }

    String escaped;
    for (const char ch : value) {
        escaped.push_back(ch);
        if (ch == '"') {
            escaped.push_back('"');
        }
    }
    return "\"" + escaped + "\"";
}

String join_identifiers(
    Backend backend,
    const std::vector<String>& values
) {
    String result;

    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            result += ", ";
        }
        result += quote_identifier(backend, values[index]);
    }

    return result;
}

String default_sql(const DefaultValue& value) {
    if (std::holds_alternative<std::monostate>(value)) {
        return {};
    }

    if (std::holds_alternative<std::nullptr_t>(value)) {
        return "NULL";
    }

    if (const auto* boolean = std::get_if<Boolean>(&value)) {
        return *boolean ? "TRUE" : "FALSE";
    }

    if (const auto* integer = std::get_if<Int64>(&value)) {
        return std::to_string(*integer);
    }

    if (const auto* integer = std::get_if<UInt64>(&value)) {
        return std::to_string(*integer);
    }

    if (const auto* number = std::get_if<Double>(&value)) {
        std::ostringstream stream;
        stream << *number;
        return stream.str();
    }

    if (const auto* string = std::get_if<String>(&value)) {
        return "'" + escape_sql_string(*string) + "'";
    }

    if (const auto* expression = std::get_if<SqlExpression>(&value)) {
        return expression->value;
    }

    return {};
}

String reference_action(ReferentialAction action) {
    switch (action) {
        case ReferentialAction::no_action: return "NO ACTION";
        case ReferentialAction::restrict_: return "RESTRICT";
        case ReferentialAction::cascade: return "CASCADE";
        case ReferentialAction::set_null: return "SET NULL";
        case ReferentialAction::set_default: return "SET DEFAULT";
    }

    return "NO ACTION";
}

String enum_check(
    Backend backend,
    const String& table,
    const ColumnDefinition& column
) {
    if (column.allowed_values.empty() || backend == Backend::mysql) {
        return {};
    }

    String result = "CHECK (" + quote_identifier(backend, column.name) + " IN (";

    for (std::size_t index = 0; index < column.allowed_values.size(); ++index) {
        if (index != 0) {
            result += ", ";
        }

        result += "'" + escape_sql_string(column.allowed_values[index]) + "'";
    }

    result += "))";

    static_cast<void>(table);
    return result;
}

String type_sql(
    Backend backend,
    const ColumnDefinition& column
) {
    const auto length = column.length == 0 ? 255U : column.length;

    switch (backend) {
        case Backend::postgresql:
            switch (column.type) {
                case ColumnType::id:
                case ColumnType::big_integer: return "BIGINT";
                case ColumnType::tiny_integer:
                case ColumnType::small_integer: return "SMALLINT";
                case ColumnType::medium_integer:
                case ColumnType::integer: return "INTEGER";
                case ColumnType::floating: return "REAL";
                case ColumnType::double_precision: return "DOUBLE PRECISION";
                case ColumnType::decimal:
                    return "NUMERIC(" + std::to_string(column.precision) + ", " +
                           std::to_string(column.scale) + ")";
                case ColumnType::boolean: return "BOOLEAN";
                case ColumnType::fixed_string:
                    return "CHAR(" + std::to_string(length) + ")";
                case ColumnType::string:
                case ColumnType::enumeration:
                    return "VARCHAR(" + std::to_string(length) + ")";
                case ColumnType::text:
                case ColumnType::medium_text:
                case ColumnType::long_text: return "TEXT";
                case ColumnType::binary: return "BYTEA";
                case ColumnType::json: return "JSONB";
                case ColumnType::uuid: return "UUID";
                case ColumnType::date: return "DATE";
                case ColumnType::time: return "TIME";
                case ColumnType::date_time:
                case ColumnType::timestamp: return "TIMESTAMP";
                case ColumnType::timestamp_tz: return "TIMESTAMPTZ";
            }
            break;

        case Backend::mysql:
            switch (column.type) {
                case ColumnType::id:
                case ColumnType::big_integer: return "BIGINT";
                case ColumnType::tiny_integer: return "TINYINT";
                case ColumnType::small_integer: return "SMALLINT";
                case ColumnType::medium_integer: return "MEDIUMINT";
                case ColumnType::integer: return "INT";
                case ColumnType::floating: return "FLOAT";
                case ColumnType::double_precision: return "DOUBLE";
                case ColumnType::decimal:
                    return "DECIMAL(" + std::to_string(column.precision) + ", " +
                           std::to_string(column.scale) + ")";
                case ColumnType::boolean: return "BOOLEAN";
                case ColumnType::fixed_string:
                    return "CHAR(" + std::to_string(length) + ")";
                case ColumnType::string:
                    return "VARCHAR(" + std::to_string(length) + ")";
                case ColumnType::text: return "TEXT";
                case ColumnType::medium_text: return "MEDIUMTEXT";
                case ColumnType::long_text: return "LONGTEXT";
                case ColumnType::binary: return "LONGBLOB";
                case ColumnType::json: return "JSON";
                case ColumnType::uuid: return "CHAR(36)";
                case ColumnType::date: return "DATE";
                case ColumnType::time: return "TIME";
                case ColumnType::date_time: return "DATETIME";
                case ColumnType::timestamp:
                case ColumnType::timestamp_tz: return "TIMESTAMP";
                case ColumnType::enumeration: {
                    String result = "ENUM(";
                    for (
                        std::size_t index = 0;
                        index < column.allowed_values.size();
                        ++index
                    ) {
                        if (index != 0) {
                            result += ", ";
                        }
                        result += "'" +
                                  escape_sql_string(column.allowed_values[index]) +
                                  "'";
                    }
                    result += ")";
                    return result;
                }
            }
            break;

        case Backend::mssql:
            switch (column.type) {
                case ColumnType::id:
                case ColumnType::big_integer: return "BIGINT";
                case ColumnType::tiny_integer: return "TINYINT";
                case ColumnType::small_integer: return "SMALLINT";
                case ColumnType::medium_integer:
                case ColumnType::integer: return "INT";
                case ColumnType::floating: return "REAL";
                case ColumnType::double_precision: return "FLOAT(53)";
                case ColumnType::decimal:
                    return "DECIMAL(" + std::to_string(column.precision) + ", " +
                           std::to_string(column.scale) + ")";
                case ColumnType::boolean: return "BIT";
                case ColumnType::fixed_string:
                    return "NCHAR(" + std::to_string(length) + ")";
                case ColumnType::string:
                case ColumnType::enumeration:
                    return "NVARCHAR(" + std::to_string(length) + ")";
                case ColumnType::text:
                case ColumnType::medium_text:
                case ColumnType::long_text:
                case ColumnType::json: return "NVARCHAR(MAX)";
                case ColumnType::binary: return "VARBINARY(MAX)";
                case ColumnType::uuid: return "UNIQUEIDENTIFIER";
                case ColumnType::date: return "DATE";
                case ColumnType::time: return "TIME";
                case ColumnType::date_time:
                case ColumnType::timestamp: return "DATETIME2";
                case ColumnType::timestamp_tz: return "DATETIMEOFFSET";
            }
            break;

        case Backend::mongodb:
            break;
    }

    throw std::logic_error("Unsupported migration column type");
}

String conventional_index_name(
    const String& table,
    const std::vector<String>& columns,
    IndexType type
) {
    String result = table;

    for (const auto& column : columns) {
        result += "_" + column;
    }

    switch (type) {
        case IndexType::primary: result += "_primary"; break;
        case IndexType::unique: result += "_unique"; break;
        case IndexType::index: result += "_index"; break;
    }

    return result;
}

String foreign_name(
    const String& table,
    const std::vector<String>& columns
) {
    String result = table;
    for (const auto& column : columns) {
        result += "_" + column;
    }
    return result + "_foreign";
}

String column_sql(
    Backend backend,
    const String& table,
    const ColumnDefinition& column
) {
    String sql = quote_identifier(backend, column.name) + " " +
                 type_sql(backend, column);

    if (backend == Backend::mysql && column.unsigned_value) {
        sql += " UNSIGNED";
    }

    if (column.auto_increment_value) {
        if (backend == Backend::postgresql) {
            sql += " GENERATED BY DEFAULT AS IDENTITY";
        } else if (backend == Backend::mysql) {
            sql += " AUTO_INCREMENT";
        } else if (backend == Backend::mssql) {
            sql += " IDENTITY(1,1)";
        }
    }

    sql += column.nullable_value ? " NULL" : " NOT NULL";

    const auto default_value = default_sql(column.default_value_data);
    if (!default_value.empty()) {
        if (backend == Backend::mssql && default_value == "TRUE") {
            sql += " DEFAULT 1";
        } else if (backend == Backend::mssql && default_value == "FALSE") {
            sql += " DEFAULT 0";
        } else {
            sql += " DEFAULT " + default_value;
        }
    }

    if (!column.collation_value.empty()) {
        sql += " COLLATE " + column.collation_value;
    }

    if (column.use_current_on_update_value && backend == Backend::mysql) {
        sql += " ON UPDATE CURRENT_TIMESTAMP";
    }

    if (column.primary_value) {
        const auto name = table + "_pkey";
        if (backend == Backend::mssql) {
            sql += " CONSTRAINT " + quote_identifier(backend, name) +
                   " PRIMARY KEY";
        } else {
            sql += " PRIMARY KEY";
        }
    }

    if (column.unique_value) {
        sql += " UNIQUE";
    }

    if (column.reference) {
        const auto constraint = foreign_name(table, {column.name});
        sql += " CONSTRAINT " + quote_identifier(backend, constraint) +
               " REFERENCES " +
               quote_identifier(backend, column.reference->table) +
               " (" + quote_identifier(backend, column.reference->column) + ")" +
               " ON DELETE " + reference_action(column.reference->on_delete) +
               " ON UPDATE " + reference_action(column.reference->on_update);
    }

    const auto check = enum_check(backend, table, column);
    if (!check.empty()) {
        sql += " " + check;
    }

    if (backend == Backend::mssql && column.type == ColumnType::json) {
        sql += " CHECK (ISJSON(" + quote_identifier(backend, column.name) +
               ") = 1)";
    }

    return sql;
}

String foreign_sql(
    Backend backend,
    const String& table,
    const ForeignKeyDefinition& foreign
) {
    const auto name = foreign.name.empty()
        ? foreign_name(table, foreign.columns)
        : foreign.name;

    return "CONSTRAINT " + quote_identifier(backend, name) +
           " FOREIGN KEY (" + join_identifiers(backend, foreign.columns) + ")" +
           " REFERENCES " + quote_identifier(backend, foreign.table) +
           " (" + join_identifiers(backend, foreign.referenced_columns) + ")" +
           " ON DELETE " + reference_action(foreign.on_delete_action) +
           " ON UPDATE " + reference_action(foreign.on_update_action);
}

Statement sql_statement(String value) {
    return Statement{StatementKind::sql, std::move(value)};
}

void append_index(
    Compilation& result,
    Backend backend,
    const String& table,
    const IndexDefinition& index
) {
    const auto name = index.name.empty()
        ? conventional_index_name(table, index.columns, index.type)
        : index.name;

    if (index.type == IndexType::primary) {
        if (backend == Backend::mysql) {
            result.statements.push_back(sql_statement(
                "ALTER TABLE " + quote_identifier(backend, table) +
                " ADD PRIMARY KEY (" +
                join_identifiers(backend, index.columns) + ");"
            ));
        } else {
            result.statements.push_back(sql_statement(
                "ALTER TABLE " + quote_identifier(backend, table) +
                " ADD CONSTRAINT " + quote_identifier(backend, name) +
                " PRIMARY KEY (" +
                join_identifiers(backend, index.columns) + ");"
            ));
        }
        return;
    }

    const auto prefix =
        index.type == IndexType::unique ? "CREATE UNIQUE INDEX " : "CREATE INDEX ";

    result.statements.push_back(sql_statement(
        prefix + quote_identifier(backend, name) +
        " ON " + quote_identifier(backend, table) +
        " (" + join_identifiers(backend, index.columns) + ");"
    ));
}

void append_column_indexes(
    Compilation& result,
    Backend backend,
    const String& table,
    const std::vector<ColumnDefinition>& columns
) {
    for (const auto& column : columns) {
        if (!column.index_value) {
            continue;
        }

        IndexDefinition definition{
            .type = IndexType::index,
            .columns = {column.name},
            .name = {}
        };
        append_index(result, backend, table, definition);
    }
}

void compile_create(
    Compilation& result,
    Backend backend,
    const TableOperation& operation
) {
    String sql = "CREATE TABLE " + quote_identifier(backend, operation.name) + " (";
    std::vector<String> definitions;

    definitions.reserve(
        operation.columns.size() + operation.foreign_keys.size()
    );

    for (const auto& column : operation.columns) {
        definitions.push_back(column_sql(backend, operation.name, column));
    }

    for (const auto& foreign : operation.foreign_keys) {
        definitions.push_back(foreign_sql(backend, operation.name, foreign));
    }

    for (std::size_t index = 0; index < definitions.size(); ++index) {
        if (index != 0) {
            sql += ", ";
        }
        sql += definitions[index];
    }

    sql += ");";
    result.statements.push_back(sql_statement(std::move(sql)));

    append_column_indexes(result, backend, operation.name, operation.columns);

    for (const auto& index : operation.indexes) {
        append_index(result, backend, operation.name, index);
    }
}

void append_alter_column(
    Compilation& result,
    Backend backend,
    const String& table,
    const ColumnDefinition& column
) {
    if (!column.change_value) {
        result.statements.push_back(sql_statement(
            "ALTER TABLE " + quote_identifier(backend, table) +
            " ADD " + column_sql(backend, table, column) + ";"
        ));
        return;
    }

    if (backend == Backend::mysql) {
        result.statements.push_back(sql_statement(
            "ALTER TABLE " + quote_identifier(backend, table) +
            " MODIFY COLUMN " + column_sql(backend, table, column) + ";"
        ));
        return;
    }

    if (backend == Backend::mssql) {
        String definition = quote_identifier(backend, column.name) + " " +
                            type_sql(backend, column) +
                            (column.nullable_value ? " NULL" : " NOT NULL");

        result.statements.push_back(sql_statement(
            "ALTER TABLE " + quote_identifier(backend, table) +
            " ALTER COLUMN " + definition + ";"
        ));
        return;
    }

    result.statements.push_back(sql_statement(
        "ALTER TABLE " + quote_identifier(backend, table) +
        " ALTER COLUMN " + quote_identifier(backend, column.name) +
        " TYPE " + type_sql(backend, column) + ";"
    ));

    result.statements.push_back(sql_statement(
        "ALTER TABLE " + quote_identifier(backend, table) +
        " ALTER COLUMN " + quote_identifier(backend, column.name) +
        (column.nullable_value ? " DROP NOT NULL;" : " SET NOT NULL;")
    ));
}

void append_alter_command(
    Compilation& result,
    Backend backend,
    const String& table,
    const AlterCommand& command
) {
    switch (command.type) {
        case AlterCommandType::drop_column:
            for (const auto& column : command.columns) {
                result.statements.push_back(sql_statement(
                    "ALTER TABLE " + quote_identifier(backend, table) +
                    " DROP COLUMN " + quote_identifier(backend, column) + ";"
                ));
            }
            return;

        case AlterCommandType::rename_column:
            if (backend == Backend::mssql) {
                result.statements.push_back(sql_statement(
                    "EXEC sp_rename N'" + table + "." + command.name +
                    "', N'" + command.new_name + "', N'COLUMN';"
                ));
            } else {
                result.statements.push_back(sql_statement(
                    "ALTER TABLE " + quote_identifier(backend, table) +
                    " RENAME COLUMN " + quote_identifier(backend, command.name) +
                    " TO " + quote_identifier(backend, command.new_name) + ";"
                ));
            }
            return;

        case AlterCommandType::drop_primary: {
            if (backend == Backend::mysql) {
                result.statements.push_back(sql_statement(
                    "ALTER TABLE " + quote_identifier(backend, table) +
                    " DROP PRIMARY KEY;"
                ));
                return;
            }

            const auto name = command.name.empty()
                ? table + "_pkey"
                : command.name;

            result.statements.push_back(sql_statement(
                "ALTER TABLE " + quote_identifier(backend, table) +
                " DROP CONSTRAINT " + quote_identifier(backend, name) + ";"
            ));
            return;
        }

        case AlterCommandType::drop_unique:
        case AlterCommandType::drop_index:
            if (backend == Backend::mysql) {
                result.statements.push_back(sql_statement(
                    "DROP INDEX " + quote_identifier(backend, command.name) +
                    " ON " + quote_identifier(backend, table) + ";"
                ));
            } else if (backend == Backend::mssql) {
                result.statements.push_back(sql_statement(
                    "DROP INDEX " + quote_identifier(backend, command.name) +
                    " ON " + quote_identifier(backend, table) + ";"
                ));
            } else {
                result.statements.push_back(sql_statement(
                    "DROP INDEX " + quote_identifier(backend, command.name) + ";"
                ));
            }
            return;

        case AlterCommandType::rename_index:
            if (backend == Backend::postgresql) {
                result.statements.push_back(sql_statement(
                    "ALTER INDEX " + quote_identifier(backend, command.name) +
                    " RENAME TO " +
                    quote_identifier(backend, command.new_name) + ";"
                ));
            } else if (backend == Backend::mysql) {
                result.statements.push_back(sql_statement(
                    "ALTER TABLE " + quote_identifier(backend, table) +
                    " RENAME INDEX " + quote_identifier(backend, command.name) +
                    " TO " + quote_identifier(backend, command.new_name) + ";"
                ));
            } else {
                result.statements.push_back(sql_statement(
                    "EXEC sp_rename N'" + table + "." + command.name +
                    "', N'" + command.new_name + "', N'INDEX';"
                ));
            }
            return;

        case AlterCommandType::drop_foreign: {
            auto name = command.name;
            if (name.empty()) {
                name = foreign_name(table, command.columns);
            }

            if (backend == Backend::mysql) {
                result.statements.push_back(sql_statement(
                    "ALTER TABLE " + quote_identifier(backend, table) +
                    " DROP FOREIGN KEY " + quote_identifier(backend, name) + ";"
                ));
            } else {
                result.statements.push_back(sql_statement(
                    "ALTER TABLE " + quote_identifier(backend, table) +
                    " DROP CONSTRAINT " + quote_identifier(backend, name) + ";"
                ));
            }
            return;
        }
    }
}

void compile_alter(
    Compilation& result,
    Backend backend,
    const TableOperation& operation
) {
    for (const auto& column : operation.columns) {
        append_alter_column(result, backend, operation.name, column);
    }

    append_column_indexes(result, backend, operation.name, operation.columns);

    for (const auto& foreign : operation.foreign_keys) {
        result.statements.push_back(sql_statement(
            "ALTER TABLE " + quote_identifier(backend, operation.name) +
            " ADD " + foreign_sql(backend, operation.name, foreign) + ";"
        ));
    }

    for (const auto& index : operation.indexes) {
        append_index(result, backend, operation.name, index);
    }

    for (const auto& command : operation.commands) {
        append_alter_command(result, backend, operation.name, command);
    }
}

Compilation compile_sql(
    const migration::Plan& plan,
    Backend backend
) {
    Compilation result;

    for (const auto& operation : plan.operations()) {
        switch (operation.type) {
            case TableOperationType::create:
                compile_create(result, backend, operation);
                break;

            case TableOperationType::alter:
                compile_alter(result, backend, operation);
                break;

            case TableOperationType::rename:
                if (backend == Backend::mssql) {
                    result.statements.push_back(sql_statement(
                        "EXEC sp_rename N'" + operation.name +
                        "', N'" + operation.new_name + "';"
                    ));
                } else {
                    result.statements.push_back(sql_statement(
                        "ALTER TABLE " + quote_identifier(backend, operation.name) +
                        " RENAME TO " +
                        quote_identifier(backend, operation.new_name) + ";"
                    ));
                }
                break;

            case TableOperationType::drop:
                result.statements.push_back(sql_statement(
                    "DROP TABLE " + quote_identifier(backend, operation.name) + ";"
                ));
                break;

            case TableOperationType::drop_if_exists:
                result.statements.push_back(sql_statement(
                    "DROP TABLE IF EXISTS " +
                    quote_identifier(backend, operation.name) + ";"
                ));
                break;
        }
    }

    return result;
}

String mongo_bson_type(ColumnType type) {
    switch (type) {
        case ColumnType::id:
        case ColumnType::tiny_integer:
        case ColumnType::small_integer:
        case ColumnType::medium_integer:
        case ColumnType::integer:
        case ColumnType::big_integer:
            return "long";

        case ColumnType::floating:
        case ColumnType::double_precision:
        case ColumnType::decimal:
            return "double";

        case ColumnType::boolean:
            return "bool";

        case ColumnType::binary:
            return "binData";

        case ColumnType::date:
        case ColumnType::time:
        case ColumnType::date_time:
        case ColumnType::timestamp:
        case ColumnType::timestamp_tz:
            return "date";

        case ColumnType::fixed_string:
        case ColumnType::string:
        case ColumnType::text:
        case ColumnType::medium_text:
        case ColumnType::long_text:
        case ColumnType::json:
        case ColumnType::uuid:
        case ColumnType::enumeration:
            return "string";
    }

    return "string";
}

String escape_json(const String& value) {
    String result;
    result.reserve(value.size());

    for (const char ch : value) {
        switch (ch) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result.push_back(ch); break;
        }
    }

    return result;
}

Statement mongo_command(String value) {
    return Statement{StatementKind::mongodb_command, std::move(value)};
}

String mongo_validator(
    const TableOperation& operation
) {
    String required;
    String properties;
    bool first_required = true;
    bool first_property = true;

    for (const auto& column : operation.columns) {
        if (column.type == ColumnType::id) {
            continue;
        }

        if (!column.nullable_value) {
            if (!first_required) {
                required += ",";
            }
            required += "\"" + escape_json(column.name) + "\"";
            first_required = false;
        }

        if (!first_property) {
            properties += ",";
        }

        properties += "\"" + escape_json(column.name) +
                      "\":{\"bsonType\":\"" +
                      mongo_bson_type(column.type) + "\"";

        if (!column.allowed_values.empty()) {
            properties += ",\"enum\":[";
            for (
                std::size_t index = 0;
                index < column.allowed_values.size();
                ++index
            ) {
                if (index != 0) {
                    properties += ",";
                }
                properties += "\"" +
                              escape_json(column.allowed_values[index]) +
                              "\"";
            }
            properties += "]";
        }

        properties += "}";
        first_property = false;
    }

    return "{\"$jsonSchema\":{\"bsonType\":\"object\",\"required\":[" +
           required + "],\"properties\":{" + properties + "}}}";
}

void append_mongo_indexes(
    Compilation& result,
    const TableOperation& operation
) {
    auto add = [&](const std::vector<String>& columns, bool unique, String name) {
        if (name.empty()) {
            name = conventional_index_name(
                operation.name,
                columns,
                unique ? IndexType::unique : IndexType::index
            );
        }

        String keys;
        for (std::size_t index = 0; index < columns.size(); ++index) {
            if (index != 0) {
                keys += ",";
            }
            keys += "\"" + escape_json(columns[index]) + "\":1";
        }

        result.statements.push_back(mongo_command(
            "{\"createIndexes\":\"" + escape_json(operation.name) +
            "\",\"indexes\":[{\"key\":{" + keys +
            "},\"name\":\"" + escape_json(name) + "\"" +
            (unique ? ",\"unique\":true" : "") + "}]}"
        ));
    };

    for (const auto& column : operation.columns) {
        if (column.index_value || column.unique_value) {
            add({column.name}, column.unique_value, {});
        }
    }

    for (const auto& index : operation.indexes) {
        if (index.type == IndexType::primary) {
            result.warnings.push_back(
                "MongoDB uses _id as its primary key; custom primary index ignored for collection '" +
                operation.name + "'"
            );
            continue;
        }

        add(
            index.columns,
            index.type == IndexType::unique,
            index.name
        );
    }
}

Compilation compile_mongodb(const migration::Plan& plan) {
    Compilation result;

    for (const auto& operation : plan.operations()) {
        switch (operation.type) {
            case TableOperationType::create:
                result.statements.push_back(mongo_command(
                    "{\"create\":\"" + escape_json(operation.name) +
                    "\",\"validator\":" + mongo_validator(operation) + "}"
                ));
                append_mongo_indexes(result, operation);

                if (
                    !operation.foreign_keys.empty() ||
                    std::any_of(
                        operation.columns.begin(),
                        operation.columns.end(),
                        [](const auto& column) {
                            return column.reference.has_value();
                        }
                    )
                ) {
                    result.warnings.push_back(
                        "MongoDB does not enforce relational foreign keys for collection '" +
                        operation.name + "'"
                    );
                }
                break;

            case TableOperationType::alter:
                if (!operation.columns.empty()) {
                    result.statements.push_back(mongo_command(
                        "{\"collMod\":\"" + escape_json(operation.name) +
                        "\",\"validator\":" + mongo_validator(operation) + "}"
                    ));
                }

                append_mongo_indexes(result, operation);

                for (const auto& command : operation.commands) {
                    if (
                        command.type == AlterCommandType::rename_column &&
                        !command.name.empty()
                    ) {
                        result.statements.push_back(mongo_command(
                            "{\"update\":\"" + escape_json(operation.name) +
                            "\",\"updates\":[{\"q\":{},\"u\":{\"$rename\":{\"" +
                            escape_json(command.name) + "\":\"" +
                            escape_json(command.new_name) +
                            "\"}},\"multi\":true}]}"
                        ));
                    } else if (
                        command.type == AlterCommandType::drop_column &&
                        !command.columns.empty()
                    ) {
                        String unset;
                        for (
                            std::size_t index = 0;
                            index < command.columns.size();
                            ++index
                        ) {
                            if (index != 0) {
                                unset += ",";
                            }
                            unset += "\"" +
                                     escape_json(command.columns[index]) +
                                     "\":\"\"";
                        }

                        result.statements.push_back(mongo_command(
                            "{\"update\":\"" + escape_json(operation.name) +
                            "\",\"updates\":[{\"q\":{},\"u\":{\"$unset\":{" +
                            unset + "}},\"multi\":true}]}"
                        ));
                    } else if (
                        command.type == AlterCommandType::drop_index ||
                        command.type == AlterCommandType::drop_unique
                    ) {
                        result.statements.push_back(mongo_command(
                            "{\"dropIndexes\":\"" +
                            escape_json(operation.name) +
                            "\",\"index\":\"" +
                            escape_json(command.name) + "\"}"
                        ));
                    }
                }

                if (
                    !operation.foreign_keys.empty() ||
                    std::any_of(
                        operation.columns.begin(),
                        operation.columns.end(),
                        [](const auto& column) {
                            return column.reference.has_value();
                        }
                    )
                ) {
                    result.warnings.push_back(
                        "MongoDB does not enforce relational foreign keys for collection '" +
                        operation.name + "'"
                    );
                }
                break;

            case TableOperationType::rename:
                result.warnings.push_back(
                    "MongoDB collection rename requires the active database name; runner must translate rename '" +
                    operation.name + "' -> '" + operation.new_name + "'"
                );
                break;

            case TableOperationType::drop:
            case TableOperationType::drop_if_exists:
                result.statements.push_back(mongo_command(
                    "{\"drop\":\"" + escape_json(operation.name) + "\"}"
                ));
                break;
        }
    }

    return result;
}

} // namespace

Compilation compile(
    const migration::Plan& plan,
    Backend backend
) {
    if (backend == Backend::mongodb) {
        return compile_mongodb(plan);
    }

    return compile_sql(plan, backend);
}

} // namespace gungnir::database
