#include <gungnir/orm/compiler.hpp>

#include <sstream>
#include <stdexcept>

namespace gungnir::orm {

namespace {

String quote_identifier(database::Backend backend, const String& value) {
    if (backend == database::Backend::mysql) {
        return "`" + value + "`";
    }

    if (backend == database::Backend::mssql) {
        return "[" + value + "]";
    }

    return "\"" + value + "\"";
}

String sql_operator(Operator value) {
    switch (value) {
        case Operator::equal: return "=";
        case Operator::not_equal: return "<>";
        case Operator::less: return "<";
        case Operator::less_or_equal: return "<=";
        case Operator::greater: return ">";
        case Operator::greater_or_equal: return ">=";
        case Operator::like: return "LIKE";
    }

    return "=";
}

String placeholder(
    database::Backend backend,
    std::size_t index
) {
    if (backend == database::Backend::postgresql) {
        return "$" + std::to_string(index + 1);
    }

    if (backend == database::Backend::mssql) {
        return "@p" + std::to_string(index + 1);
    }

    return "?";
}

String mongo_operator(Operator value) {
    switch (value) {
        case Operator::equal: return "$eq";
        case Operator::not_equal: return "$ne";
        case Operator::less: return "$lt";
        case Operator::less_or_equal: return "$lte";
        case Operator::greater: return "$gt";
        case Operator::greater_or_equal: return "$gte";
        case Operator::like: return "$regex";
    }

    return "$eq";
}

void append_sql_predicate(
    CompiledQuery& result,
    database::Backend backend,
    const Predicate& predicate
) {
    result.text += quote_identifier(backend, predicate.column);

    switch (predicate.kind) {
        case PredicateKind::comparison:
            result.text += " " + sql_operator(predicate.comparison) + " ";
            result.text += placeholder(backend, result.bindings.size());
            result.bindings.push_back(predicate.values.front());
            break;

        case PredicateKind::in:
        case PredicateKind::not_in:
            result.text += predicate.kind == PredicateKind::in
                ? " IN ("
                : " NOT IN (";

            if (predicate.values.empty()) {
                result.text += "NULL";
            } else {
                for (
                    std::size_t index = 0;
                    index < predicate.values.size();
                    ++index
                ) {
                    if (index != 0) {
                        result.text += ", ";
                    }
                    result.text += placeholder(backend, result.bindings.size());
                    result.bindings.push_back(predicate.values[index]);
                }
            }

            result.text += ")";
            break;

        case PredicateKind::null_:
            result.text += " IS NULL";
            break;

        case PredicateKind::not_null:
            result.text += " IS NOT NULL";
            break;
    }
}

CompiledQuery compile_sql(
    const QueryPlan& plan,
    database::Backend backend
) {
    CompiledQuery result;
    result.kind = CompiledQueryKind::sql;
    result.text = "SELECT ";

    if (plan.columns.empty()) {
        result.text += "*";
    } else {
        for (std::size_t index = 0; index < plan.columns.size(); ++index) {
            if (index != 0) {
                result.text += ", ";
            }
            result.text += quote_identifier(backend, plan.columns[index]);
        }
    }

    result.text += " FROM " + quote_identifier(backend, plan.table);

    if (!plan.predicates.empty()) {
        result.text += " WHERE ";

        for (
            std::size_t index = 0;
            index < plan.predicates.size();
            ++index
        ) {
            const auto& predicate = plan.predicates[index];

            if (index != 0) {
                result.text += predicate.connector == BooleanConnector::and_
                    ? " AND "
                    : " OR ";
            }

            append_sql_predicate(result, backend, predicate);
        }
    }

    if (!plan.orders.empty()) {
        result.text += " ORDER BY ";

        for (std::size_t index = 0; index < plan.orders.size(); ++index) {
            if (index != 0) {
                result.text += ", ";
            }

            result.text += quote_identifier(
                backend,
                plan.orders[index].column
            );

            result.text += plan.orders[index].direction == Direction::asc
                ? " ASC"
                : " DESC";
        }
    }

    if (plan.limit) {
        if (backend == database::Backend::mssql) {
            if (plan.orders.empty()) {
                result.text += " ORDER BY (SELECT NULL)";
            }

            result.text += " OFFSET " +
                           std::to_string(plan.offset.value_or(0)) +
                           " ROWS FETCH NEXT " +
                           std::to_string(*plan.limit) +
                           " ROWS ONLY";
        } else {
            result.text += " LIMIT " + std::to_string(*plan.limit);

            if (plan.offset) {
                result.text += " OFFSET " + std::to_string(*plan.offset);
            }
        }
    } else if (plan.offset) {
        if (backend == database::Backend::mssql) {
            if (plan.orders.empty()) {
                result.text += " ORDER BY (SELECT NULL)";
            }

            result.text += " OFFSET " +
                           std::to_string(*plan.offset) +
                           " ROWS";
        } else {
            result.text += " OFFSET " + std::to_string(*plan.offset);
        }
    }

    result.text += ";";
    return result;
}

CompiledQuery compile_mongodb(const QueryPlan& plan) {
    CompiledQuery result;
    result.kind = CompiledQueryKind::mongodb;
    result.text = "{\"find\":\"" + plan.table + "\",\"filter\":{";

    for (std::size_t index = 0; index < plan.predicates.size(); ++index) {
        const auto& predicate = plan.predicates[index];

        if (index != 0) {
            result.text += ",";
        }

        result.text += "\"" + predicate.column + "\":";

        switch (predicate.kind) {
            case PredicateKind::comparison:
                result.text += "{\"" + mongo_operator(predicate.comparison) +
                               "\":{\"$bind\":" +
                               std::to_string(result.bindings.size()) + "}}";
                result.bindings.push_back(predicate.values.front());
                break;

            case PredicateKind::in:
            case PredicateKind::not_in: {
                result.text += "{\"";
                result.text += predicate.kind == PredicateKind::in
                    ? "$in"
                    : "$nin";
                result.text += "\":[";

                for (
                    std::size_t value_index = 0;
                    value_index < predicate.values.size();
                    ++value_index
                ) {
                    if (value_index != 0) {
                        result.text += ",";
                    }

                    result.text += "{\"$bind\":" +
                                   std::to_string(result.bindings.size()) +
                                   "}";
                    result.bindings.push_back(
                        predicate.values[value_index]
                    );
                }

                result.text += "]}";
                break;
            }

            case PredicateKind::null_:
                result.text += "{\"$eq\":null}";
                break;

            case PredicateKind::not_null:
                result.text += "{\"$ne\":null}";
                break;
        }
    }

    result.text += "}";

    if (!plan.columns.empty()) {
        result.text += ",\"projection\":{";

        for (std::size_t index = 0; index < plan.columns.size(); ++index) {
            if (index != 0) {
                result.text += ",";
            }

            result.text += "\"" + plan.columns[index] + "\":1";
        }

        result.text += "}";
    }

    if (!plan.orders.empty()) {
        result.text += ",\"sort\":{";

        for (std::size_t index = 0; index < plan.orders.size(); ++index) {
            if (index != 0) {
                result.text += ",";
            }

            result.text += "\"" + plan.orders[index].column + "\":" +
                           (
                               plan.orders[index].direction == Direction::asc
                               ? "1"
                               : "-1"
                           );
        }

        result.text += "}";
    }

    if (plan.limit) {
        result.text += ",\"limit\":" + std::to_string(*plan.limit);
    }

    if (plan.offset) {
        result.text += ",\"skip\":" + std::to_string(*plan.offset);
    }

    result.text += "}";
    return result;
}

} // namespace

CompiledQuery compile(
    const QueryPlan& plan,
    database::Backend backend
) {
    if (plan.type != QueryType::select) {
        throw std::logic_error("Unsupported Gungnir ORM query type");
    }

    if (backend == database::Backend::mongodb) {
        return compile_mongodb(plan);
    }

    return compile_sql(plan, backend);
}

} // namespace gungnir::orm
