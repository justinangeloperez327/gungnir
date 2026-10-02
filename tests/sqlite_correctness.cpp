#include <cassert>
#include <memory>
#include <string>

#include <gungnir/database/database.hpp>
#include <gungnir/database/sqlite.hpp>
#include <gungnir/model/decimal.hpp>

int main() {
    using namespace gungnir;
    using namespace gungnir::database;

    Settings settings;
    settings.name = "default";
    settings.backend = Backend::sqlite;
    settings.database = ":memory:";
    settings.pool_size = 1;

    Manager manager;

    manager.add(
        "default",
        Backend::sqlite,
        [settings] {
            return
                std::make_shared<
                    SQLiteDriver
                >(settings);
        },
        PoolOptions{
            .size = 1,
            .acquire_timeout =
                std::chrono::
                    milliseconds{250},
            .validation_interval =
                std::chrono::
                    seconds{30},
            .reconnect_attempts = 1
        }
    );

    {
        auto connection =
            manager.connection();

        connection->execute(
            "CREATE TABLE parents ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT NOT NULL)"
        );

        connection->execute(
            "CREATE TABLE children ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "parent_id INTEGER NOT NULL, "
            "amount TEXT, "
            "note TEXT, "
            "FOREIGN KEY(parent_id) "
            "REFERENCES parents(id))"
        );
    }

    assert(
        manager.pool_stats()
            .leased == 0
    );

    {
        auto connection =
            manager.connection();

        Transaction transaction{
            connection
        };

        const auto parent =
            connection->execute(
                "INSERT INTO parents(name) "
                "VALUES (?)",
                {
                    String{"committed"}
                }
            );

        assert(
            parent.inserted_id
                .has_value()
        );

        transaction.commit();

        assert(!transaction.active());
    }

    {
        auto connection =
            manager.connection();

        Transaction transaction{
            connection
        };

        connection->execute(
            "INSERT INTO parents(name) "
            "VALUES (?)",
            {
                String{"rolled-back"}
            }
        );

        transaction.rollback();
    }

    {
        auto connection =
            manager.connection();

        const auto count =
            connection->execute(
                "SELECT COUNT(*) AS total "
                "FROM parents"
            );

        assert(count.rows.size() == 1);
        assert(
            std::get<Int64>(
                count.rows[0]
                    .at("total")
            ) == 1
        );
    }

    {
        auto connection =
            manager.connection();

        Transaction outer{
            connection
        };

        connection->execute(
            "INSERT INTO children("
            "parent_id, amount, note) "
            "VALUES (?, ?, ?)",
            {
                Int64{1},
                model::Decimal{
                    "123.4500"
                },
                nullptr
            }
        );

        {
            Transaction inner{
                connection
            };

            connection->execute(
                "INSERT INTO children("
                "parent_id, amount, note) "
                "VALUES (?, ?, ?)",
                {
                    Int64{1},
                    model::Decimal{
                        "999.000"
                    },
                    String{"nested"}
                }
            );

            inner.rollback();
        }

        outer.commit();
    }

    {
        auto connection =
            manager.connection();

        const auto rows =
            connection->execute(
                "SELECT amount, note "
                "FROM children "
                "ORDER BY id"
            );

        assert(rows.rows.size() == 1);

        assert(
            std::get<String>(
                rows.rows[0]
                    .at("amount")
            ) ==
            "123.4500"
        );

        assert(
            std::holds_alternative<
                std::nullptr_t
            >(
                rows.rows[0]
                    .at("note")
            )
        );

        bool foreign_key_rejected =
            false;

        try {
            static_cast<void>(
                connection->execute(
                    "INSERT INTO children("
                    "parent_id, amount) "
                    "VALUES (?, ?)",
                    {
                        Int64{999},
                        String{"1.0"}
                    }
                )
            );
        } catch (
            const Error&
        ) {
            foreign_key_rejected =
                true;
        }

        assert(
            foreign_key_rejected
        );
    }

    const auto stats =
        manager.pool_stats();

    assert(stats.leased == 0);
    assert(stats.available == 1);
}
