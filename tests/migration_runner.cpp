#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>

#include <gungnir/database/database.hpp>
#include <gungnir/migration/runner.hpp>

class CreateUsers : public gungnir::Migration {
public:
    void up() override {
        Table::create("users", [](Column& column) {
            column.id();
            column.string("name");
        });
    }

    void down() override {
        Table::drop_if_exists("users");
    }
};

class MemoryRepository final : public gungnir::migration::Repository {
public:
    void ensure(gungnir::database::Connection&) override {}

    std::vector<gungnir::migration::Record> applied(
        gungnir::database::Connection&
    ) override {
        return records;
    }

    std::size_t last_batch(
        gungnir::database::Connection&
    ) override {
        std::size_t result = 0;
        for (const auto& record : records) {
            result = std::max(result, record.batch);
        }
        return result;
    }

    void record(
        gungnir::database::Connection&,
        const gungnir::migration::Record& migration
    ) override {
        records.push_back(migration);
    }

    void remove(
        gungnir::database::Connection&,
        std::string_view name
    ) override {
        std::erase_if(records, [&](const auto& record) {
            return record.name == name;
        });
    }

    std::vector<gungnir::migration::Record> records;
};

int main() {
    using namespace gungnir;

    int begins = 0;
    int commits = 0;
    std::vector<String> statements;

    database::Manager manager;
    manager.add(
        "default",
        database::Backend::postgresql,
        [&] {
            return std::make_shared<database::CallbackDriver>(
                database::Backend::postgresql,
                [&](const String& statement, const auto&) {
                    statements.push_back(statement);
                    return database::Result{
                        .rows = {},
                        .affected_rows = 1,
                        .inserted_id = {}
                    };
                },
                [&] { ++begins; },
                [&] { ++commits; },
                [] {}
            );
        }
    );

    database::runtime::use(manager);

    CreateUsers create_users;
    MemoryRepository repository;
    migration::Runner runner{repository};

    const std::vector<migration::Named> migrations{
        {"2026_09_21_create_users", &create_users}
    };

    assert(runner.migrate(migrations) == 1);
    assert(repository.records.size() == 1);
    assert(runner.migrate(migrations) == 0);

    assert(runner.rollback(migrations) == 1);
    assert(repository.records.empty());

    assert(begins == 2);
    assert(commits == 2);
    assert(statements.size() == 2);

    database::runtime::clear();
    return 0;
}
