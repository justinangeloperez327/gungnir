#include "benchmark.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gungnir/database/backend.hpp>
#include <gungnir/orm/compiler.hpp>
#include <gungnir/orm/plan.hpp>

namespace {

gungnir::orm::QueryPlan
make_sql_plan() {
    using namespace gungnir;
    using namespace gungnir::orm;

    QueryPlan plan;
    plan.table = "users";
    plan.columns = {
        "users.id",
        "users.name",
        "users.email",
        "profiles.country"
    };

    plan.joins.push_back(
        Join{
            .type = JoinType::left,
            .table = "profiles",
            .first = "profiles.user_id",
            .comparison =
                Comparison::equal,
            .second = "users.id"
        }
    );

    for (
        std::size_t index = 0;
        index < 24;
        ++index
    ) {
        plan.predicates.push_back(
            Predicate{
                .kind =
                    PredicateKind::
                        comparison,
                .connector =
                    BooleanConnector::and_,
                .column =
                    "score_" +
                    std::to_string(index),
                .comparison =
                    Comparison::
                        greater_or_equal,
                .values = {
                    Int64{
                        static_cast<Int64>(
                            index
                        )
                    }
                }
            }
        );
    }

    std::vector<
        model::AttributeValue
    > ids;

    ids.reserve(32);

    for (
        std::size_t index = 0;
        index < 32;
        ++index
    ) {
        ids.emplace_back(
            Int64{
                static_cast<Int64>(
                    index + 1
                )
            }
        );
    }

    plan.predicates.push_back(
        Predicate{
            .kind =
                PredicateKind::in_list,
            .connector =
                BooleanConnector::and_,
            .column = "users.id",
            .comparison =
                Comparison::equal,
            .values = std::move(ids)
        }
    );

    plan.groups = {
        "users.id",
        "profiles.country"
    };

    plan.having.push_back(
        Predicate{
            .kind =
                PredicateKind::
                    comparison,
            .connector =
                BooleanConnector::and_,
            .column = "users.id",
            .comparison =
                Comparison::
                    greater_than,
            .values = {
                Int64{0}
            }
        }
    );

    plan.orders = {
        Order{
            .column = "users.name",
            .direction =
                SortDirection::asc
        },
        Order{
            .column = "users.id",
            .direction =
                SortDirection::desc
        }
    };

    plan.limit = 100;
    plan.offset = 200;

    return plan;
}

gungnir::orm::QueryPlan
make_mongodb_plan() {
    using namespace gungnir;
    using namespace gungnir::orm;

    QueryPlan plan;
    plan.table = "events";
    plan.columns = {
        "id",
        "type",
        "tenant_id",
        "created_at"
    };

    for (
        std::size_t index = 0;
        index < 20;
        ++index
    ) {
        plan.predicates.push_back(
            Predicate{
                .kind =
                    PredicateKind::
                        comparison,
                .connector =
                    BooleanConnector::and_,
                .column =
                    "metric_" +
                    std::to_string(index),
                .comparison =
                    Comparison::
                        greater_or_equal,
                .values = {
                    Int64{
                        static_cast<Int64>(
                            index
                        )
                    }
                }
            }
        );
    }

    plan.orders = {
        Order{
            .column = "created_at",
            .direction =
                SortDirection::desc
        }
    };

    plan.limit = 50;
    plan.offset = 25;

    return plan;
}

std::uint64_t compile_case(
    const gungnir::orm::QueryPlan& plan,
    gungnir::database::Backend backend,
    std::size_t iteration
) {
    const auto query =
        gungnir::orm::compile(
            plan,
            backend
        );

    return
        static_cast<std::uint64_t>(
            query.text.size() +
            query.bindings.size()
        ) +
        static_cast<std::uint64_t>(
            iteration
        );
}

} // namespace

int main(
    int argc,
    char** argv
) {
    const auto config =
        gungnir::benchmark::
            parse_config(
                argc,
                argv
            );

    const auto sql_plan =
        make_sql_plan();

    const auto mongodb_plan =
        make_mongodb_plan();

    gungnir::benchmark::Suite suite{
        "orm-query-compiler",
        config
    };

    suite.run(
        "postgresql_complex_select",
        [&](std::size_t iteration) {
            return compile_case(
                sql_plan,
                gungnir::database::Backend::
                    postgresql,
                iteration
            );
        }
    );

    suite.run(
        "mysql_complex_select",
        [&](std::size_t iteration) {
            return compile_case(
                sql_plan,
                gungnir::database::Backend::
                    mysql,
                iteration
            );
        }
    );

    suite.run(
        "sqlserver_complex_select",
        [&](std::size_t iteration) {
            return compile_case(
                sql_plan,
                gungnir::database::Backend::
                    mssql,
                iteration
            );
        }
    );

    suite.run(
        "mongodb_complex_find",
        [&](std::size_t iteration) {
            return compile_case(
                mongodb_plan,
                gungnir::database::Backend::
                    mongodb,
                iteration
            );
        }
    );

    return suite.finish();
}
