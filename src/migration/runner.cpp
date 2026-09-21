#include <gungnir/migration/runner.hpp>

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <gungnir/database/compiler.hpp>
#include <gungnir/database/runtime.hpp>
#include <gungnir/database/transaction.hpp>

namespace gungnir::migration {

namespace {

String quote(database::Backend backend, std::string_view value) {
    if (backend == database::Backend::mysql) {
        return "`" + String{value} + "`";
    }

    if (backend == database::Backend::mssql) {
        return "[" + String{value} + "]";
    }

    return "\"" + String{value} + "\"";
}

String placeholder(database::Backend backend, std::size_t index) {
    if (backend == database::Backend::postgresql) {
        return "$" + std::to_string(index + 1);
    }

    if (backend == database::Backend::mssql) {
        return "@p" + std::to_string(index + 1);
    }

    return "?";
}

std::size_t record_batch(const model::AttributeMap& row) {
    const auto found = row.find("batch");
    if (found == row.end()) {
        return 0;
    }

    if (const auto* value = std::get_if<Int64>(&found->second)) {
        return static_cast<std::size_t>(*value);
    }

    if (const auto* value = std::get_if<UInt64>(&found->second)) {
        return static_cast<std::size_t>(*value);
    }

    return 0;
}

String record_name(const model::AttributeMap& row) {
    const auto found = row.find("migration");
    if (found == row.end()) {
        return {};
    }

    if (const auto* value = std::get_if<String>(&found->second)) {
        return *value;
    }

    return {};
}

} // namespace

void DatabaseRepository::ensure(database::Connection& connection) {
    if (connection.backend() == database::Backend::mongodb) {
        connection.execute(
            "{\"create\":\"gungnir_migrations\"}"
        );
        return;
    }

    const auto migration = quote(connection.backend(), "migration");
    const auto batch = quote(connection.backend(), "batch");
    const auto table = quote(connection.backend(), "gungnir_migrations");

    if (connection.backend() == database::Backend::mssql) {
        connection.execute(
            "IF OBJECT_ID(N'gungnir_migrations', N'U') IS NULL "
            "CREATE TABLE " + table +
            " (" + migration + " NVARCHAR(255) NOT NULL PRIMARY KEY, " +
            batch + " BIGINT NOT NULL);"
        );
        return;
    }

    connection.execute(
        "CREATE TABLE IF NOT EXISTS " + table +
        " (" + migration + " VARCHAR(255) NOT NULL PRIMARY KEY, " +
        batch + " BIGINT NOT NULL);"
    );
}

std::vector<Record> DatabaseRepository::applied(
    database::Connection& connection
) {
    database::Result result;

    if (connection.backend() == database::Backend::mongodb) {
        result = connection.execute(
            "{\"find\":\"gungnir_migrations\",\"filter\":{},"
            "\"sort\":{\"batch\":1}}"
        );
    } else {
        result = connection.execute(
            "SELECT " + quote(connection.backend(), "migration") +
            ", " + quote(connection.backend(), "batch") +
            " FROM " +
            quote(connection.backend(), "gungnir_migrations") +
            " ORDER BY " + quote(connection.backend(), "batch") +
            " ASC;"
        );
    }

    std::vector<Record> records;
    records.reserve(result.rows.size());

    for (const auto& row : result.rows) {
        records.push_back(Record{
            .name = record_name(row),
            .batch = record_batch(row)
        });
    }

    return records;
}

std::size_t DatabaseRepository::last_batch(
    database::Connection& connection
) {
    const auto records = applied(connection);
    std::size_t result = 0;

    for (const auto& record : records) {
        result = std::max(result, record.batch);
    }

    return result;
}

void DatabaseRepository::record(
    database::Connection& connection,
    const Record& migration
) {
    if (connection.backend() == database::Backend::mongodb) {
        connection.execute(
            "{\"insert\":\"gungnir_migrations\","
            "\"documents\":[{\"migration\":{\"$bind\":0},"
            "\"batch\":{\"$bind\":1}}]}",
            {
                migration.name,
                static_cast<UInt64>(migration.batch)
            }
        );
        return;
    }

    connection.execute(
        "INSERT INTO " +
        quote(connection.backend(), "gungnir_migrations") +
        " (" + quote(connection.backend(), "migration") +
        ", " + quote(connection.backend(), "batch") +
        ") VALUES (" +
        placeholder(connection.backend(), 0) + ", " +
        placeholder(connection.backend(), 1) + ");",
        {
            migration.name,
            static_cast<UInt64>(migration.batch)
        }
    );
}

void DatabaseRepository::remove(
    database::Connection& connection,
    std::string_view name
) {
    if (connection.backend() == database::Backend::mongodb) {
        connection.execute(
            "{\"delete\":\"gungnir_migrations\","
            "\"deletes\":[{\"q\":{\"migration\":{\"$bind\":0}},"
            "\"limit\":1}]}",
            {String{name}}
        );
        return;
    }

    connection.execute(
        "DELETE FROM " +
        quote(connection.backend(), "gungnir_migrations") +
        " WHERE " + quote(connection.backend(), "migration") +
        " = " + placeholder(connection.backend(), 0) + ";",
        {String{name}}
    );
}

Runner::Runner(
    Repository& repository,
    String connection
)
    : repository_(repository),
      connection_(std::move(connection)) {}

void Runner::execute_plan(
    database::Connection& connection,
    const Plan& plan
) {
    const auto compiled = database::compile(
        plan,
        connection.backend()
    );

    for (const auto& statement : compiled.statements) {
        connection.execute(statement.text);
    }
}

std::size_t Runner::migrate(
    const std::vector<Named>& migrations
) {
    auto connection = database::runtime::connection(connection_);
    repository_.ensure(*connection);

    const auto records = repository_.applied(*connection);
    std::unordered_set<String> applied_names;

    for (const auto& record : records) {
        applied_names.insert(record.name);
    }

    const auto batch = repository_.last_batch(*connection) + 1;
    std::size_t count = 0;

    for (const auto& item : migrations) {
        if (
            item.migration == nullptr ||
            applied_names.contains(item.name)
        ) {
            continue;
        }

        database::Transaction transaction{connection};

        execute_plan(
            transaction.connection(),
            item.migration->plan_up()
        );

        repository_.record(
            transaction.connection(),
            Record{item.name, batch}
        );

        transaction.commit();
        ++count;
    }

    return count;
}

std::size_t Runner::rollback(
    const std::vector<Named>& migrations
) {
    auto connection = database::runtime::connection(connection_);
    repository_.ensure(*connection);

    const auto records = repository_.applied(*connection);
    if (records.empty()) {
        return 0;
    }

    const auto batch = repository_.last_batch(*connection);
    std::size_t count = 0;

    for (auto record = records.rbegin(); record != records.rend(); ++record) {
        if (record->batch != batch) {
            continue;
        }

        const auto migration = std::find_if(
            migrations.begin(),
            migrations.end(),
            [&](const Named& item) {
                return item.name == record->name;
            }
        );

        if (
            migration == migrations.end() ||
            migration->migration == nullptr
        ) {
            throw std::logic_error(
                "Applied migration '" + record->name +
                "' is not registered with the runner"
            );
        }

        database::Transaction transaction{connection};

        execute_plan(
            transaction.connection(),
            migration->migration->plan_down()
        );

        repository_.remove(
            transaction.connection(),
            record->name
        );

        transaction.commit();
        ++count;
    }

    return count;
}

std::size_t Runner::reset(
    const std::vector<Named>& migrations
) {
    std::size_t total = 0;

    while (true) {
        const auto rolled_back = rollback(migrations);
        if (rolled_back == 0) {
            break;
        }

        total += rolled_back;
    }

    return total;
}

std::vector<Status> Runner::status(
    const std::vector<Named>& migrations
) {
    auto connection =
        database::runtime::connection(
            connection_
        );

    repository_.ensure(
        *connection
    );

    const auto records =
        repository_.applied(
            *connection
        );

    std::unordered_map<
        String,
        std::size_t
    > applied;

    for (const auto& record : records) {
        applied.insert_or_assign(
            record.name,
            record.batch
        );
    }

    std::vector<Status> result;
    result.reserve(
        migrations.size()
    );

    for (const auto& item : migrations) {
        const auto found =
            applied.find(
                item.name
            );

        result.push_back(Status{
            .name = item.name,
            .applied =
                found != applied.end(),
            .batch =
                found == applied.end()
                    ? 0
                    : found->second
        });
    }

    return result;
}

} // namespace gungnir::migration
