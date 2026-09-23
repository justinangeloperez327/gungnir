#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include <gungnir/database/database.hpp>
#include <gungnir/database/sqlserver.hpp>
#include <gungnir/migration/migrations.hpp>
#include <gungnir/orm/compiler.hpp>

namespace {

class CreateSqlServerMigrationProbe :
    public gungnir::Migration {
public:
    void up() override {
        Table::create(
            "gungnir_sqlserver_migration_probe",
            [](Column& column) {
                column.id();
                column.string("name");
            }
        );
    }

    void down() override {
        Table::drop_if_exists(
            "gungnir_sqlserver_migration_probe"
        );
    }
};

gungnir::String env(
    const char* name,
    gungnir::String fallback
) {
    const auto* value =
        std::getenv(name);

    return
        value == nullptr
            ? std::move(fallback)
            : gungnir::String{value};
}

std::uint16_t port() {
    return static_cast<std::uint16_t>(
        std::stoul(
            env(
                "GUNGNIR_SQLSERVER_PORT",
                "1433"
            )
        )
    );
}

} // namespace

int main() {
    using namespace gungnir;

    database::Settings settings;
    settings.name =
        "sqlserver_integration";
    settings.backend =
        database::Backend::mssql;
    settings.host =
        env(
            "GUNGNIR_SQLSERVER_HOST",
            "127.0.0.1"
        );
    settings.port = port();
    settings.database =
        env(
            "GUNGNIR_SQLSERVER_DATABASE",
            "master"
        );
    settings.username =
        env(
            "GUNGNIR_SQLSERVER_USERNAME",
            "sa"
        );
    settings.password =
        env(
            "GUNGNIR_SQLSERVER_PASSWORD",
            ""
        );
    settings.options =
        "TrustServerCertificate=yes";

    database::DriverRegistry registry;
    database::register_sqlserver(
        registry
    );

    assert(
        registry.has(
            database::Backend::mssql
        )
    );

    database::Manager manager;
    manager.add(
        settings.name,
        settings.backend,
        registry.bind(settings),
        2
    );

    auto connection =
        manager.connection(
            settings.name
        );

    assert(connection->healthy());

    connection->execute(
        "IF OBJECT_ID(N'dbo.gungnir_sqlserver_integration', N'U') "
        "IS NOT NULL DROP TABLE dbo.gungnir_sqlserver_integration"
    );

    connection->execute(
        "CREATE TABLE dbo.gungnir_sqlserver_integration ("
        "id BIGINT IDENTITY(1,1) PRIMARY KEY,"
        "email NVARCHAR(255) NOT NULL UNIQUE,"
        "active BIT NOT NULL,"
        "score FLOAT(53) NOT NULL,"
        "note NVARCHAR(MAX) NULL"
        ")"
    );

    const auto inserted =
        connection->execute(
            "INSERT INTO dbo.gungnir_sqlserver_integration "
            "(email, active, score, note) "
            "OUTPUT INSERTED.id, INSERTED.email, INSERTED.active, "
            "INSERTED.score, INSERTED.note "
            "VALUES (?, ?, ?, ?)",
            {
                String{"first@example.com"},
                Boolean{true},
                Double{12.5},
                nullptr
            }
        );

    assert(inserted.rows.size() == 1);
    assert(inserted.inserted_id.has_value());

    const auto& first =
        inserted.rows.front();

    assert(
        model::value_cast<String>(
            first.at("email")
        ) ==
        "first@example.com"
    );

    assert(
        model::value_cast<Boolean>(
            first.at("active")
        )
    );

    assert(
        model::value_cast<Double>(
            first.at("score")
        ) ==
        12.5
    );

    assert(
        std::holds_alternative<
            std::nullptr_t
        >(
            first.at("note")
        )
    );

    orm::QueryPlan orm_plan;
    orm_plan.table =
        "dbo.gungnir_sqlserver_integration";
    orm_plan.columns = {
        "id",
        "email",
        "active",
        "score",
        "note"
    };
    orm_plan.predicates.push_back(
        orm::Predicate{
            .kind =
                orm::PredicateKind::comparison,
            .connector =
                orm::BooleanConnector::and_,
            .column = "email",
            .comparison =
                orm::Comparison::equal,
            .other_column = {},
            .values = {
                String{"first@example.com"}
            },
            .automatic = false
        }
    );

    const auto orm_query =
        orm::compile(
            orm_plan,
            database::Backend::mssql
        );

    assert(
        orm_query.text.find('?') !=
        String::npos
    );
    assert(
        orm_query.text.find("@p") ==
        String::npos
    );
    assert(
        orm_query.bindings.size() == 1
    );

    const auto orm_selected =
        connection->execute(
            orm_query.text,
            orm_query.bindings
        );

    assert(orm_selected.rows.size() == 1);
    assert(
        model::value_cast<String>(
            orm_selected.rows.front().at(
                "email"
            )
        ) ==
        "first@example.com"
    );

    connection->begin();

    connection->execute(
        "INSERT INTO dbo.gungnir_sqlserver_integration "
        "(email, active, score, note) "
        "VALUES (?, ?, ?, ?)",
        {
            String{
                "rolled-back@example.com"
            },
            Boolean{false},
            Double{1.0},
            String{"temporary"}
        }
    );

    connection->rollback();

    const auto rolled_back =
        connection->execute(
            "SELECT COUNT_BIG(*) AS count "
            "FROM dbo.gungnir_sqlserver_integration "
            "WHERE email = ?",
            {
                String{
                    "rolled-back@example.com"
                }
            }
        );

    assert(
        model::value_cast<Int64>(
            rolled_back.rows.front().at(
                "count"
            )
        ) ==
        0
    );

    connection->begin();

    connection->execute(
        "INSERT INTO dbo.gungnir_sqlserver_integration "
        "(email, active, score, note) "
        "VALUES (?, ?, ?, ?)",
        {
            String{
                "committed@example.com"
            },
            Boolean{false},
            Double{4.25},
            String{"persisted"}
        }
    );

    connection->commit();

    const auto committed =
        connection->execute(
            "SELECT note "
            "FROM dbo.gungnir_sqlserver_integration "
            "WHERE email = ?",
            {
                String{
                    "committed@example.com"
                }
            }
        );

    assert(committed.rows.size() == 1);
    assert(
        model::value_cast<String>(
            committed.rows.front().at(
                "note"
            )
        ) ==
        "persisted"
    );

    connection->execute(
        "DROP TABLE dbo.gungnir_sqlserver_integration"
    );

    connection->execute(
        "DROP TABLE IF EXISTS dbo.gungnir_sqlserver_migration_probe"
    );
    connection->execute(
        "DROP TABLE IF EXISTS dbo.gungnir_migrations"
    );

    database::runtime::use(manager);

    CreateSqlServerMigrationProbe migration_probe;
    migration::Registry migration_registry;
    migration_registry.add(
        "2026_09_23_sqlserver_probe",
        migration_probe
    );

    migration::DatabaseRepository migration_repository;
    migration::Runner migration_runner{
        migration_repository,
        settings.name
    };

    const auto& migrations =
        migration_registry.all();

    assert(
        migration_runner.migrate(
            migrations
        ) == 1
    );

    const auto migration_status =
        migration_runner.status(
            migrations
        );

    assert(migration_status.size() == 1);
    assert(migration_status.front().applied);
    assert(migration_status.front().batch == 1);

    const auto table_exists =
        connection->execute(
            "SELECT COUNT_BIG(*) AS count "
            "FROM sys.tables "
            "WHERE name = ?",
            {
                String{
                    "gungnir_sqlserver_migration_probe"
                }
            }
        );

    assert(
        model::value_cast<Int64>(
            table_exists.rows.front().at(
                "count"
            )
        ) == 1
    );

    assert(
        migration_runner.rollback(
            migrations
        ) == 1
    );

    const auto rolled_back_status =
        migration_runner.status(
            migrations
        );

    assert(
        rolled_back_status.size() == 1
    );
    assert(
        !rolled_back_status.front().applied
    );

    database::runtime::clear();

    connection->execute(
        "DROP TABLE IF EXISTS dbo.gungnir_migrations"
    );

    return 0;
}
