#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <utility>

#include <gungnir/database/database.hpp>
#include <gungnir/database/mysql.hpp>

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
                "GUNGNIR_MYSQL_PORT",
                "3306"
            )
        )
    );
}

} // namespace

int main() {
    using namespace gungnir;

    database::Settings settings;
    settings.name = "mysql_integration";
    settings.backend =
        database::Backend::mysql;
    settings.host =
        env(
            "GUNGNIR_MYSQL_HOST",
            "127.0.0.1"
        );
    settings.port = port();
    settings.database =
        env(
            "GUNGNIR_MYSQL_DATABASE",
            "gungnir"
        );
    settings.username =
        env(
            "GUNGNIR_MYSQL_USERNAME",
            "root"
        );
    settings.password =
        env(
            "GUNGNIR_MYSQL_PASSWORD",
            "mysql"
        );

    database::DriverRegistry registry;
    database::register_mysql(
        registry
    );

    assert(
        registry.has(
            database::Backend::mysql
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
        "DROP TABLE IF EXISTS gungnir_mysql_integration"
    );

    connection->execute(
        "CREATE TABLE gungnir_mysql_integration ("
        "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,"
        "email VARCHAR(255) NOT NULL UNIQUE,"
        "active BOOLEAN NOT NULL,"
        "score DOUBLE NOT NULL,"
        "note TEXT NULL"
        ")"
    );

    const auto inserted =
        connection->execute(
            "INSERT INTO gungnir_mysql_integration "
            "(email, active, score, note) "
            "VALUES (?, ?, ?, ?)",
            {
                String{"first@example.com"},
                Boolean{true},
                Double{12.5},
                nullptr
            }
        );

    assert(inserted.affected_rows == 1);
    assert(inserted.inserted_id.has_value());

    const auto inserted_id =
        model::value_cast<UInt64>(
            *inserted.inserted_id
        );

    const auto selected =
        connection->execute(
            "SELECT id, email, active, score, note "
            "FROM gungnir_mysql_integration "
            "WHERE id = ?",
            {
                inserted_id
            }
        );

    assert(selected.rows.size() == 1);

    const auto& first =
        selected.rows.front();

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
        "INSERT INTO gungnir_mysql_integration "
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
            "SELECT COUNT(*) AS count "
            "FROM gungnir_mysql_integration "
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
        "INSERT INTO gungnir_mysql_integration "
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
            "FROM gungnir_mysql_integration "
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
        "DROP TABLE gungnir_mysql_integration"
    );

    return 0;
}
