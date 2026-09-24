#include <gungnir/orm/advanced_mutation.hpp>

#include <algorithm>
#include <stdexcept>

namespace gungnir::orm {

namespace {

String quote_segment(database::Backend backend, const String& value) {
    if (backend == database::Backend::mysql) {
        return "`" + value + "`";
    }
    if (backend == database::Backend::mssql) {
        return "[" + value + "]";
    }
    return "\"" + value + "\"";
}

String quote_path(database::Backend backend, const String& value) {
    String result;
    std::size_t start = 0;

    while (start <= value.size()) {
        const auto dot = value.find('.', start);
        const auto length = dot == String::npos
            ? value.size() - start
            : dot - start;

        if (!result.empty()) {
            result += ".";
        }

        result += quote_segment(
            backend,
            value.substr(start, length)
        );

        if (dot == String::npos) {
            break;
        }

        start = dot + 1;
    }

    return result;
}

String placeholder(database::Backend backend, std::size_t index) {
    if (backend == database::Backend::postgresql) {
        return "$" + std::to_string(index + 1);
    }
    return "?";
}

String sql_operator(Comparison value) {
    switch (value) {
        case Comparison::equal: return "=";
        case Comparison::not_equal: return "<>";
        case Comparison::less_than: return "<";
        case Comparison::less_or_equal: return "<=";
        case Comparison::greater_than: return ">";
        case Comparison::greater_or_equal: return ">=";
        case Comparison::like: return "LIKE";
    }
    return "=";
}

void ensure_mutable_plan(const QueryPlan& plan) {
    if (
        !plan.joins.empty() ||
        !plan.groups.empty() ||
        !plan.having.empty() ||
        plan.aggregate
    ) {
        throw std::logic_error(
            "Bulk mutation cannot be combined with joins, groups, having, or aggregates"
        );
    }
}

void append_where_sql(
    CompiledQuery& result,
    const QueryPlan& plan,
    database::Backend backend
) {
    if (plan.predicates.empty()) {
        return;
    }

    result.text += " WHERE ";

    for (std::size_t index = 0; index < plan.predicates.size(); ++index) {
        const auto& predicate = plan.predicates[index];

        if (index != 0) {
            result.text += predicate.connector == BooleanConnector::and_
                ? " AND "
                : " OR ";
        }

        if (
            (predicate.kind == PredicateKind::in_list ||
             predicate.kind == PredicateKind::not_in_list) &&
            predicate.values.empty()
        ) {
            result.text += predicate.kind == PredicateKind::in_list
                ? "1 = 0"
                : "1 = 1";
            continue;
        }

        result.text += quote_path(backend, predicate.column);

        switch (predicate.kind) {
            case PredicateKind::comparison:
                result.text += " " + sql_operator(predicate.comparison) + " " +
                               placeholder(backend, result.bindings.size());
                result.bindings.push_back(predicate.values.front());
                break;

            case PredicateKind::column_comparison:
                result.text += " " + sql_operator(predicate.comparison) + " " +
                               quote_path(backend, predicate.other_column);
                break;

            case PredicateKind::in_list:
            case PredicateKind::not_in_list:
                result.text += predicate.kind == PredicateKind::in_list
                    ? " IN ("
                    : " NOT IN (";

                for (
                    std::size_t value_index = 0;
                    value_index < predicate.values.size();
                    ++value_index
                ) {
                    if (value_index != 0) {
                        result.text += ", ";
                    }
                    result.text += placeholder(
                        backend,
                        result.bindings.size()
                    );
                    result.bindings.push_back(
                        predicate.values[value_index]
                    );
                }
                result.text += ")";
                break;

            case PredicateKind::between:
            case PredicateKind::not_between:
                if (predicate.values.size() != 2) {
                    throw std::logic_error(
                        "ORM between predicate requires exactly two values"
                    );
                }
                result.text += predicate.kind == PredicateKind::between
                    ? " BETWEEN "
                    : " NOT BETWEEN ";
                result.text += placeholder(
                    backend,
                    result.bindings.size()
                );
                result.bindings.push_back(predicate.values[0]);
                result.text += " AND ";
                result.text += placeholder(
                    backend,
                    result.bindings.size()
                );
                result.bindings.push_back(predicate.values[1]);
                break;

            case PredicateKind::is_null:
                result.text += " IS NULL";
                break;

            case PredicateKind::is_not_null:
                result.text += " IS NOT NULL";
                break;
        }
    }
}

String mongo_field(std::string_view value) {
    return value == "id"
        ? String{"_id"}
        : String{value};
}

String mongo_bind(
    CompiledQuery& result,
    const model::AttributeValue& value
) {
    const auto index = result.bindings.size();
    result.bindings.push_back(value);
    return "{\"$bind\":" + std::to_string(index) + "}";
}

String mongo_predicate(
    CompiledQuery& result,
    const Predicate& predicate
) {
    auto op = [&] {
        switch (predicate.comparison) {
            case Comparison::equal: return String{"$eq"};
            case Comparison::not_equal: return String{"$ne"};
            case Comparison::less_than: return String{"$lt"};
            case Comparison::less_or_equal: return String{"$lte"};
            case Comparison::greater_than: return String{"$gt"};
            case Comparison::greater_or_equal: return String{"$gte"};
            case Comparison::like: return String{"$regex"};
        }
        return String{"$eq"};
    }();

    if (predicate.kind == PredicateKind::column_comparison) {
        return "{\"$expr\":{\"" + op + "\":[\"$" +
               mongo_field(predicate.column) + "\",\"$" +
               mongo_field(predicate.other_column) + "\"]}}";
    }

    if (
        predicate.kind == PredicateKind::is_null ||
        predicate.kind == PredicateKind::is_not_null
    ) {
        return "{\"" + mongo_field(predicate.column) + "\":{\"" +
               (predicate.kind == PredicateKind::is_null ? "$eq" : "$ne") +
               "\":null}}";
    }

    if (
        predicate.kind == PredicateKind::in_list ||
        predicate.kind == PredicateKind::not_in_list
    ) {
        String values = "[";
        for (std::size_t index = 0; index < predicate.values.size(); ++index) {
            if (index != 0) {
                values += ",";
            }
            values += mongo_bind(result, predicate.values[index]);
        }
        values += "]";
        return "{\"" + mongo_field(predicate.column) + "\":{\"" +
               (predicate.kind == PredicateKind::in_list ? "$in" : "$nin") +
               "\":" + values + "}}";
    }

    if (
        predicate.kind == PredicateKind::between ||
        predicate.kind == PredicateKind::not_between
    ) {
        if (predicate.values.size() != 2) {
            throw std::logic_error(
                "ORM between predicate requires exactly two values"
            );
        }

        const auto range = "{\"$gte\":" +
                           mongo_bind(result, predicate.values[0]) +
                           ",\"$lte\":" +
                           mongo_bind(result, predicate.values[1]) + "}";

        if (predicate.kind == PredicateKind::not_between) {
            return "{\"" + mongo_field(predicate.column) +
                   "\":{\"$not\":" + range + "}}";
        }

        return "{\"" + mongo_field(predicate.column) + "\":" + range + "}";
    }

    return "{\"" + mongo_field(predicate.column) + "\":{\"" + op + "\":" +
           mongo_bind(result, predicate.values.front()) + "}}";
}

String mongo_filter(
    CompiledQuery& result,
    const QueryPlan& plan
) {
    if (plan.predicates.empty()) {
        return "{}";
    }

    String expression = mongo_predicate(result, plan.predicates.front());

    for (std::size_t index = 1; index < plan.predicates.size(); ++index) {
        const auto next = mongo_predicate(result, plan.predicates[index]);
        const auto op =
            plan.predicates[index].connector == BooleanConnector::and_
            ? "$and"
            : "$or";

        expression = "{\"" + String{op} + "\":[" +
                     expression + "," + next + "]}";
    }

    return expression;
}

std::vector<String> sorted_keys(const model::AttributeMap& row) {
    std::vector<String> keys;
    keys.reserve(row.size());

    for (const auto& [key, value] : row) {
        static_cast<void>(value);
        keys.push_back(key);
    }

    std::sort(keys.begin(), keys.end());
    return keys;
}

} // namespace

CompiledQuery compile_update_where(
    const QueryPlan& plan,
    const model::AttributeMap& values,
    database::Backend backend
) {
    ensure_mutable_plan(plan);

    if (values.empty()) {
        throw std::invalid_argument(
            "ORM bulk update requires at least one value"
        );
    }

    CompiledQuery result;
    const auto keys = sorted_keys(values);

    if (backend == database::Backend::mongodb) {
        result.kind = CompiledQueryKind::mongodb;
        const auto filter = mongo_filter(result, plan);
        result.text = "{\"update\":\"" + plan.table +
                      "\",\"updates\":[{\"q\":" + filter +
                      ",\"u\":{\"$set\":{";

        for (std::size_t index = 0; index < keys.size(); ++index) {
            if (index != 0) {
                result.text += ",";
            }
            result.text += "\"" + mongo_field(keys[index]) + "\":" +
                           mongo_bind(result, values.at(keys[index]));
        }

        result.text += "}},\"multi\":true}]}";
        return result;
    }

    result.kind = CompiledQueryKind::sql;
    result.text = "UPDATE " + quote_path(backend, plan.table) + " SET ";

    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            result.text += ", ";
        }
        result.text += quote_path(backend, keys[index]) + " = " +
                       placeholder(backend, result.bindings.size());
        result.bindings.push_back(values.at(keys[index]));
    }

    append_where_sql(result, plan, backend);
    result.text += ";";
    return result;
}

CompiledQuery compile_delete_where(
    const QueryPlan& plan,
    database::Backend backend
) {
    ensure_mutable_plan(plan);
    CompiledQuery result;

    if (backend == database::Backend::mongodb) {
        result.kind = CompiledQueryKind::mongodb;
        const auto filter = mongo_filter(result, plan);
        result.text = "{\"delete\":\"" + plan.table +
                      "\",\"deletes\":[{\"q\":" + filter +
                      ",\"limit\":0}]}";
        return result;
    }

    result.kind = CompiledQueryKind::sql;
    result.text = "DELETE FROM " + quote_path(backend, plan.table);
    append_where_sql(result, plan, backend);
    result.text += ";";
    return result;
}

CompiledQuery compile_increment_where(
    const QueryPlan& plan,
    String column,
    Double amount,
    database::Backend backend
) {
    ensure_mutable_plan(plan);
    CompiledQuery result;

    if (backend == database::Backend::mongodb) {
        result.kind = CompiledQueryKind::mongodb;
        const auto filter = mongo_filter(result, plan);
        result.text = "{\"update\":\"" + plan.table +
                      "\",\"updates\":[{\"q\":" + filter +
                      ",\"u\":{\"$inc\":{\"" + column +
                      "\":" + mongo_bind(result, amount) +
                      "}},\"multi\":true}]}";
        return result;
    }

    result.kind = CompiledQueryKind::sql;
    result.text = "UPDATE " + quote_path(backend, plan.table) +
                  " SET " + quote_path(backend, column) + " = " +
                  quote_path(backend, column) + " + " +
                  placeholder(backend, result.bindings.size());
    result.bindings.push_back(amount);
    append_where_sql(result, plan, backend);
    result.text += ";";
    return result;
}

CompiledQuery compile_insert_many(
    String table,
    const std::vector<model::AttributeMap>& rows,
    database::Backend backend
) {
    if (rows.empty()) {
        throw std::invalid_argument(
            "ORM bulk insert requires at least one row"
        );
    }

    const auto keys = sorted_keys(rows.front());

    for (const auto& row : rows) {
        if (sorted_keys(row) != keys) {
            throw std::invalid_argument(
                "ORM bulk insert rows must have the same attributes"
            );
        }
    }

    CompiledQuery result;

    if (backend == database::Backend::mongodb) {
        result.kind = CompiledQueryKind::mongodb;
        result.text = "{\"insert\":\"" + table + "\",\"documents\":[";

        for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
            if (row_index != 0) {
                result.text += ",";
            }
            result.text += "{";

            for (std::size_t key_index = 0; key_index < keys.size(); ++key_index) {
                if (key_index != 0) {
                    result.text += ",";
                }
                result.text += "\"" + mongo_field(keys[key_index]) + "\":" +
                               mongo_bind(
                                   result,
                                   rows[row_index].at(keys[key_index])
                               );
            }

            result.text += "}";
        }

        result.text += "]}";
        return result;
    }

    result.kind = CompiledQueryKind::sql;
    result.text = "INSERT INTO " + quote_path(backend, table) + " (";

    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            result.text += ", ";
        }
        result.text += quote_path(backend, keys[index]);
    }

    result.text += ") VALUES ";

    for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
        if (row_index != 0) {
            result.text += ", ";
        }
        result.text += "(";

        for (std::size_t key_index = 0; key_index < keys.size(); ++key_index) {
            if (key_index != 0) {
                result.text += ", ";
            }

            result.text += placeholder(
                backend,
                result.bindings.size()
            );
            result.bindings.push_back(
                rows[row_index].at(keys[key_index])
            );
        }

        result.text += ")";
    }

    result.text += ";";
    return result;
}


CompiledQuery compile_upsert(
    String table,
    const std::vector<model::AttributeMap>& rows,
    const std::vector<String>& unique_by,
    const std::vector<String>& update_columns,
    database::Backend backend,
    bool ignore_conflicts
) {
    if (rows.empty()) {
        throw std::invalid_argument(
            "ORM upsert requires at least one row"
        );
    }

    if (unique_by.empty()) {
        throw std::invalid_argument(
            "ORM upsert requires at least one unique key"
        );
    }

    const auto keys = sorted_keys(rows.front());

    for (const auto& row : rows) {
        if (sorted_keys(row) != keys) {
            throw std::invalid_argument(
                "ORM upsert rows must have the same attributes"
            );
        }
    }

    for (const auto& key : unique_by) {
        if (!rows.front().contains(key)) {
            throw std::invalid_argument(
                "ORM upsert unique key is missing from row attributes"
            );
        }
    }

    auto updates = update_columns;
    if (updates.empty() && !ignore_conflicts) {
        for (const auto& key : keys) {
            if (
                std::find(unique_by.begin(), unique_by.end(), key) ==
                unique_by.end()
            ) {
                updates.push_back(key);
            }
        }
    }

    CompiledQuery result;

    if (backend == database::Backend::mongodb) {
        result.kind = CompiledQueryKind::mongodb;
        result.text = "{\"update\":\"" + table + "\",\"updates\":[";

        for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
            if (row_index != 0) {
                result.text += ",";
            }

            result.text += "{\"q\":{";
            for (std::size_t key_index = 0; key_index < unique_by.size(); ++key_index) {
                if (key_index != 0) {
                    result.text += ",";
                }

                const auto& key = unique_by[key_index];
                result.text += "\"" + mongo_field(key) + "\":" +
                               mongo_bind(result, rows[row_index].at(key));
            }
            result.text += ignore_conflicts
                ? "},\"u\":{\"$setOnInsert\":{"
                : "},\"u\":{\"$set\":{";

            for (std::size_t key_index = 0; key_index < keys.size(); ++key_index) {
                if (key_index != 0) {
                    result.text += ",";
                }

                const auto& key = keys[key_index];
                result.text += "\"" + mongo_field(key) + "\":" +
                               mongo_bind(result, rows[row_index].at(key));
            }

            result.text += "}},\"upsert\":true,\"multi\":false}";
        }

        result.text += "]}";
        return result;
    }

    if (backend == database::Backend::mssql) {
        result.kind = CompiledQueryKind::sql;
        result.text = "MERGE INTO " + quote_path(backend, table) +
                      " AS target USING (VALUES ";

        for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
            if (row_index != 0) {
                result.text += ", ";
            }
            result.text += "(";

            for (std::size_t key_index = 0; key_index < keys.size(); ++key_index) {
                if (key_index != 0) {
                    result.text += ", ";
                }
                result.text += placeholder(
                    backend,
                    result.bindings.size()
                );
                result.bindings.push_back(
                    rows[row_index].at(keys[key_index])
                );
            }

            result.text += ")";
        }

        result.text += ") AS source (";
        for (std::size_t index = 0; index < keys.size(); ++index) {
            if (index != 0) {
                result.text += ", ";
            }
            result.text += quote_path(backend, keys[index]);
        }
        result.text += ") ON ";

        for (std::size_t index = 0; index < unique_by.size(); ++index) {
            if (index != 0) {
                result.text += " AND ";
            }
            result.text += "target." + quote_path(backend, unique_by[index]) +
                           " = source." + quote_path(backend, unique_by[index]);
        }

        if (!ignore_conflicts && !updates.empty()) {
            result.text += " WHEN MATCHED THEN UPDATE SET ";
            for (std::size_t index = 0; index < updates.size(); ++index) {
                if (index != 0) {
                    result.text += ", ";
                }
                result.text += "target." + quote_path(backend, updates[index]) +
                               " = source." + quote_path(backend, updates[index]);
            }
        }

        result.text += " WHEN NOT MATCHED THEN INSERT (";
        for (std::size_t index = 0; index < keys.size(); ++index) {
            if (index != 0) {
                result.text += ", ";
            }
            result.text += quote_path(backend, keys[index]);
        }
        result.text += ") VALUES (";
        for (std::size_t index = 0; index < keys.size(); ++index) {
            if (index != 0) {
                result.text += ", ";
            }
            result.text += "source." + quote_path(backend, keys[index]);
        }
        result.text += ");";
        return result;
    }

    result = compile_insert_many(table, rows, backend);
    if (!result.text.empty() && result.text.back() == ';') {
        result.text.pop_back();
    }

    if (backend == database::Backend::postgresql) {
        result.text += " ON CONFLICT (";
        for (std::size_t index = 0; index < unique_by.size(); ++index) {
            if (index != 0) {
                result.text += ", ";
            }
            result.text += quote_path(backend, unique_by[index]);
        }

        if (ignore_conflicts || updates.empty()) {
            result.text += ") DO NOTHING;";
            return result;
        }

        result.text += ") DO UPDATE SET ";
        for (std::size_t index = 0; index < updates.size(); ++index) {
            if (index != 0) {
                result.text += ", ";
            }
            result.text += quote_path(backend, updates[index]) +
                           " = EXCLUDED." +
                           quote_path(backend, updates[index]);
        }
        result.text += ";";
        return result;
    }

    // MySQL resolves conflicts through an existing PRIMARY or UNIQUE key.
    if (ignore_conflicts || updates.empty()) {
        // MySQL has no PostgreSQL-style DO NOTHING for duplicate-key inserts.
        // A no-op key assignment preserves the existing row.
        const auto& key = unique_by.front();
        result.text += " ON DUPLICATE KEY UPDATE " +
                       quote_path(backend, key) + " = " +
                       quote_path(backend, key) + ";";
        return result;
    }

    result.text += " AS gungnir_new ON DUPLICATE KEY UPDATE ";
    for (std::size_t index = 0; index < updates.size(); ++index) {
        if (index != 0) {
            result.text += ", ";
        }
        result.text += quote_path(backend, updates[index]) +
                       " = gungnir_new." +
                       quote_path(backend, updates[index]);
    }
    result.text += ";";
    return result;
}

CompiledQuery compile_soft_delete_where(
    const QueryPlan& plan,
    String column,
    bool restore,
    database::Backend backend
) {
    ensure_mutable_plan(plan);
    CompiledQuery result;

    if (backend == database::Backend::mongodb) {
        result.kind = CompiledQueryKind::mongodb;
        const auto filter = mongo_filter(result, plan);

        if (restore) {
            result.text = "{\"update\":\"" + plan.table +
                          "\",\"updates\":[{\"q\":" + filter +
                          ",\"u\":{\"$set\":{\"" + column +
                          "\":null}},\"multi\":true}]}";
        } else {
            result.text = "{\"update\":\"" + plan.table +
                          "\",\"updates\":[{\"q\":" + filter +
                          ",\"u\":{\"$currentDate\":{\"" + column +
                          "\":true}},\"multi\":true}]}";
        }

        return result;
    }

    result.kind = CompiledQueryKind::sql;
    result.text = "UPDATE " + quote_path(backend, plan.table) +
                  " SET " + quote_path(backend, column) +
                  (restore ? " = NULL" : " = CURRENT_TIMESTAMP");
    append_where_sql(result, plan, backend);
    result.text += ";";
    return result;
}

} // namespace gungnir::orm
