#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include <gungnir/database/database.hpp>
#include <gungnir/database/sqlserver.hpp>

namespace {

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

    return 0;
}
