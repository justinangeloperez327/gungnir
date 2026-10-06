#pragma once

#include <cassert>
#include <memory>
#include <stdexcept>

#include <gungnir/database/database.hpp>

// Run against the real adapter's existing integration table. These probes
// distinguish rollback of an inner savepoint from rollback of its parent.
inline void verify_sql_transaction_scopes(
    const std::shared_ptr<gungnir::database::Connection>& connection,
    const gungnir::String& table
) {
    using namespace gungnir;
    const bool postgres = connection->backend() == database::Backend::postgresql;
    const String parameter = postgres ? "$1" : "?";
    const String insert = "INSERT INTO " + table + " (email, active, score, note) VALUES (" +
        (postgres ? "$1, $2, $3, $4" : "?, ?, ?, ?") + ")";
    const String select = "SELECT " + String{connection->backend() == database::Backend::mssql
        ? "COUNT_BIG(*)" : "COUNT(*)"} + " AS count FROM " + table + " WHERE email = " + parameter;
    const auto write = [&](String marker) {
        connection->execute(insert, {std::move(marker), Boolean{true}, Double{1.25}, nullptr});
    };
    const auto count = [&](const String& marker) {
        return model::value_cast<Int64>(connection->execute(select, {marker}).rows.front().at("count"));
    };
    assert(connection->supports_transactions() && connection->supports_savepoints());
    int committed = 0, discarded = 0;
    database::Transaction outer{connection};
    outer.run([&] {
        write("scope-outer@example.test");
        connection->after_commit([&] { ++committed; });
        database::Transaction nested{connection};
        nested.run([&] {
            write("scope-nested@example.test");
            connection->after_commit([&] { ++committed; });
        });
        assert(committed == 0);
        try {
            database::Transaction inner{connection};
            inner.run([&] {
                write("scope-discarded@example.test");
                connection->after_commit([&] { ++discarded; });
                assert(count("scope-discarded@example.test") == 1);
                throw std::runtime_error("discard only this savepoint");
            });
        } catch (const std::runtime_error& error) {
            assert(String{error.what()} == "discard only this savepoint");
        }
        assert(count("scope-discarded@example.test") == 0);
        assert(count("scope-outer@example.test") == 1);
    });
    assert(committed == 2 && discarded == 0);
    assert(count("scope-outer@example.test") == 1);
    assert(count("scope-nested@example.test") == 1);
    try {
        database::Transaction rollback{connection};
        rollback.run([&] {
            write("scope-rollback@example.test");
            database::Transaction inner{connection};
            inner.run([&] {
                write("scope-inner-rollback@example.test");
                connection->after_commit([&] { ++discarded; });
            });
            assert(count("scope-rollback@example.test") == 1);
            assert(count("scope-inner-rollback@example.test") == 1);
            throw std::runtime_error("roll back the parent after nested commit");
        });
    } catch (const std::runtime_error& error) {
        assert(String{error.what()} == "roll back the parent after nested commit");
    }
    assert(count("scope-rollback@example.test") == 0);
    assert(count("scope-inner-rollback@example.test") == 0);
    assert(discarded == 0 && !connection->in_transaction() && connection->healthy());
}
