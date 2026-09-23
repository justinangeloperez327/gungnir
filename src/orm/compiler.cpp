#include <gungnir/orm/compiler.hpp>

#include <stdexcept>

namespace gungnir::orm {

namespace {

String quote_segment(database::Backend backend, const String& value) {
    if (value == "*") {
        return value;
    }

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
        const auto segment = value.substr(start, length);

        if (!result.empty()) {
            result += ".";
        }

        result += quote_segment(backend, segment);

        if (dot == String::npos) {
            break;
        }

        start = dot + 1;
    }

    return result;
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

String placeholder(
    database::Backend backend,
    std::size_t index
) {
    if (backend == database::Backend::postgresql) {
        return "$" + std::to_string(index + 1);
    }

    return "?";
}

String aggregate_name(AggregateFunction function) {
    switch (function) {
        case AggregateFunction::count: return "COUNT";
        case AggregateFunction::sum: return "SUM";
        case AggregateFunction::average: return "AVG";
        case AggregateFunction::minimum: return "MIN";
        case AggregateFunction::maximum: return "MAX";
    }

    return "COUNT";
}

String join_name(JoinType type) {
    switch (type) {
        case JoinType::inner: return "INNER JOIN";
        case JoinType::left: return "LEFT JOIN";
        case JoinType::right: return "RIGHT JOIN";
        case JoinType::cross: return "CROSS JOIN";
    }

    return "INNER JOIN";
}

void append_sql_predicate(
    CompiledQuery& result,
    database::Backend backend,
    const Predicate& predicate
) {
    if (
        (predicate.kind == PredicateKind::in_list ||
         predicate.kind == PredicateKind::not_in_list) &&
        predicate.values.empty()
    ) {
        result.text += predicate.kind == PredicateKind::in_list
            ? "1 = 0"
            : "1 = 1";
        return;
    }

    result.text += quote_path(backend, predicate.column);

    switch (predicate.kind) {
        case PredicateKind::comparison:
            if (predicate.values.empty()) {
                throw std::logic_error(
                    "ORM comparison predicate requires a value"
                );
            }
            result.text += " " + sql_operator(predicate.comparison) + " ";
            result.text += placeholder(backend, result.bindings.size());
            result.bindings.push_back(predicate.values.front());
            break;

        case PredicateKind::column_comparison:
            if (predicate.other_column.empty()) {
                throw std::logic_error(
                    "ORM column comparison requires a second column"
                );
            }
            result.text += " " + sql_operator(predicate.comparison) + " ";
            result.text += quote_path(backend, predicate.other_column);
            break;

        case PredicateKind::in_list:
        case PredicateKind::not_in_list:
            result.text += predicate.kind == PredicateKind::in_list
                ? " IN ("
                : " NOT IN (";

            for (
                std::size_t index = 0;
                index < predicate.values.size();
                ++index
            ) {
                if (index != 0) {
                    result.text += ", ";
                }

                result.text += placeholder(
                    backend,
                    result.bindings.size()
                );
                result.bindings.push_back(predicate.values[index]);
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
            result.text += placeholder(backend, result.bindings.size());
            result.bindings.push_back(predicate.values[0]);
            result.text += " AND ";
            result.text += placeholder(backend, result.bindings.size());
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

void append_predicates(
    CompiledQuery& result,
    database::Backend backend,
    const std::vector<Predicate>& predicates
) {
    for (std::size_t index = 0; index < predicates.size(); ++index) {
        if (index != 0) {
            result.text +=
                predicates[index].connector == BooleanConnector::and_
                ? " AND "
                : " OR ";
        }

        append_sql_predicate(result, backend, predicates[index]);
    }
}

CompiledQuery compile_sql(
    const QueryPlan& plan,
    database::Backend backend
) {
    CompiledQuery result;
    result.kind = CompiledQueryKind::sql;
    result.text = "SELECT ";

    if (plan.distinct) {
        result.text += "DISTINCT ";
    }

    if (plan.aggregate) {
        result.text += aggregate_name(plan.aggregate->function) + "(";
        result.text += plan.aggregate->column == "*"
            ? "*"
            : quote_path(backend, plan.aggregate->column);
        result.text += ") AS " + quote_segment(backend, "aggregate");
    } else if (plan.columns.empty()) {
        result.text += "*";
    } else {
        for (std::size_t index = 0; index < plan.columns.size(); ++index) {
            if (index != 0) {
                result.text += ", ";
            }
            result.text += quote_path(backend, plan.columns[index]);
        }
    }

    result.text += " FROM " + quote_path(backend, plan.table);

    if (backend == database::Backend::mssql) {
        if (plan.lock == LockMode::for_update) {
            result.text += " WITH (UPDLOCK, ROWLOCK)";
        } else if (plan.lock == LockMode::shared) {
            result.text += " WITH (HOLDLOCK, ROWLOCK)";
        }
    }

    for (const auto& join : plan.joins) {
        result.text += " " + join_name(join.type) + " " +
                       quote_path(backend, join.table);

        if (join.type != JoinType::cross) {
            result.text += " ON " + quote_path(backend, join.first) +
                           " " + sql_operator(join.comparison) + " " +
                           quote_path(backend, join.second);
        }
    }

    if (!plan.predicates.empty()) {
        result.text += " WHERE ";
        append_predicates(result, backend, plan.predicates);
    }

    if (!plan.groups.empty()) {
        result.text += " GROUP BY ";

        for (std::size_t index = 0; index < plan.groups.size(); ++index) {
            if (index != 0) {
                result.text += ", ";
            }
            result.text += quote_path(backend, plan.groups[index]);
        }
    }

    if (!plan.having.empty()) {
        result.text += " HAVING ";
        append_predicates(result, backend, plan.having);
    }

    if (!plan.orders.empty()) {
        result.text += " ORDER BY ";

        for (std::size_t index = 0; index < plan.orders.size(); ++index) {
            if (index != 0) {
                result.text += ", ";
            }

            result.text += quote_path(
                backend,
                plan.orders[index].column
            );

            result.text += plan.orders[index].direction == SortDirection::asc
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

    if (backend != database::Backend::mssql) {
        if (plan.lock == LockMode::for_update) {
            result.text += " FOR UPDATE";
        } else if (plan.lock == LockMode::shared) {
            result.text += " FOR SHARE";
        }
    }

    result.text += ";";
    return result;
}

String mongo_operator(Comparison value) {
    switch (value) {
        case Comparison::equal: return "$eq";
        case Comparison::not_equal: return "$ne";
        case Comparison::less_than: return "$lt";
        case Comparison::less_or_equal: return "$lte";
        case Comparison::greater_than: return "$gt";
        case Comparison::greater_or_equal: return "$gte";
        case Comparison::like: return "$regex";
    }

    return "$eq";
}

String mongo_bind(
    std::vector<model::AttributeValue>& bindings,
    const model::AttributeValue& value
) {
    const auto index = bindings.size();
    bindings.push_back(value);
    return "{\"$bind\":" + std::to_string(index) + "}";
}

String mongo_predicate(
    const Predicate& predicate,
    std::vector<model::AttributeValue>& bindings
) {
    if (
        predicate.kind == PredicateKind::in_list ||
        predicate.kind == PredicateKind::not_in_list
    ) {
        String values = "[";
        for (std::size_t index = 0; index < predicate.values.size(); ++index) {
            if (index != 0) {
                values += ",";
            }
            values += mongo_bind(bindings, predicate.values[index]);
        }
        values += "]";

        return "{\"" + predicate.column + "\":{\"" +
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

        const auto lower = mongo_bind(bindings, predicate.values[0]);
        const auto upper = mongo_bind(bindings, predicate.values[1]);
        const auto range = "{\"$gte\":" + lower +
                           ",\"$lte\":" + upper + "}";

        if (predicate.kind == PredicateKind::not_between) {
            return "{\"" + predicate.column +
                   "\":{\"$not\":" + range + "}}";
        }

        return "{\"" + predicate.column + "\":" + range + "}";
    }

    if (predicate.kind == PredicateKind::is_null) {
        return "{\"" + predicate.column + "\":{\"$eq\":null}}";
    }

    if (predicate.kind == PredicateKind::is_not_null) {
        return "{\"" + predicate.column + "\":{\"$ne\":null}}";
    }

    if (predicate.kind == PredicateKind::column_comparison) {
        return "{\"$expr\":{\"" + mongo_operator(predicate.comparison) +
               "\":[\"$" + predicate.column + "\",\"$" +
               predicate.other_column + "\"]}}";
    }

    if (predicate.values.empty()) {
        throw std::logic_error(
            "ORM comparison predicate requires a value"
        );
    }

    return "{\"" + predicate.column + "\":{\"" +
           mongo_operator(predicate.comparison) + "\":" +
           mongo_bind(bindings, predicate.values.front()) + "}}";
}

String mongo_filter(
    const std::vector<Predicate>& predicates,
    std::vector<model::AttributeValue>& bindings
) {
    if (predicates.empty()) {
        return "{}";
    }

    String expression = mongo_predicate(predicates.front(), bindings);

    for (std::size_t index = 1; index < predicates.size(); ++index) {
        const auto next = mongo_predicate(predicates[index], bindings);
        const auto connector =
            predicates[index].connector == BooleanConnector::and_
            ? "$and"
            : "$or";

        expression = "{\"" + String{connector} + "\":[" +
                     expression + "," + next + "]}";
    }

    return expression;
}

CompiledQuery compile_mongodb(const QueryPlan& plan) {
    if (plan.lock != LockMode::none) {
        throw std::logic_error(
            "MongoDB document queries do not support SQL row-lock clauses"
        );
    }

    if (!plan.joins.empty()) {
        throw std::logic_error(
            "MongoDB queries do not support SQL join(); use model relationships and with()"
        );
    }

    if (!plan.groups.empty() || !plan.having.empty()) {
        throw std::logic_error(
            "MongoDB group_by/having is not part of the document query surface"
        );
    }

    if (plan.distinct && !plan.aggregate) {
        throw std::logic_error(
            "MongoDB model queries do not support SQL-style distinct model hydration"
        );
    }

    CompiledQuery result;
    result.kind = CompiledQueryKind::mongodb;
    const auto filter = mongo_filter(plan.predicates, result.bindings);

    if (plan.aggregate) {
        String accumulator;

        switch (plan.aggregate->function) {
            case AggregateFunction::count:
                accumulator = "{\"$sum\":1}";
                break;
            case AggregateFunction::sum:
                accumulator = "{\"$sum\":\"$" +
                              plan.aggregate->column + "\"}";
                break;
            case AggregateFunction::average:
                accumulator = "{\"$avg\":\"$" +
                              plan.aggregate->column + "\"}";
                break;
            case AggregateFunction::minimum:
                accumulator = "{\"$min\":\"$" +
                              plan.aggregate->column + "\"}";
                break;
            case AggregateFunction::maximum:
                accumulator = "{\"$max\":\"$" +
                              plan.aggregate->column + "\"}";
                break;
        }

        result.text = "{\"aggregate\":\"" + plan.table +
                      "\",\"pipeline\":[{\"$match\":" + filter +
                      "},{\"$group\":{\"_id\":null,\"aggregate\":" +
                      accumulator + "}}],\"cursor\":{}}";
        return result;
    }

    result.text = "{\"find\":\"" + plan.table +
                  "\",\"filter\":" + filter;

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
                               plan.orders[index].direction ==
                                       SortDirection::asc
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
    if (backend == database::Backend::mongodb) {
        return compile_mongodb(plan);
    }

    return compile_sql(plan, backend);
}

} // namespace gungnir::orm
