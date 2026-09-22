#include <cassert>
#include <memory>
#include <string>
#include <vector>

#include <gungnir/database/database.hpp>

int main() {
    using namespace gungnir;

    std::vector<String> calls;
    int began = 0;
    int committed = 0;
    int rolled_back = 0;

    database::Manager manager;
    manager.add(
        "default",
        database::Backend::postgresql,
        [&] {
            return std::make_shared<database::CallbackDriver>(
                database::Backend::postgresql,
                [&](const String& statement, const auto&) {
                    calls.push_back(statement);
                    return database::Result{
                        .rows = {},
                        .affected_rows = 1,
                        .inserted_id = {}
                    };
                },
                [&] { ++began; },
                [&] { ++committed; },
                [&] { ++rolled_back; },
                [] { return true; }
            );
        },
        2
    );

    assert(manager.has("default"));
    assert(manager.backend() == database::Backend::postgresql);

    auto first = manager.connection();
    auto second = manager.connection();

    assert(first != second);
    assert(first->healthy());

    first->execute("SELECT 1");
    assert(calls.size() == 1);

    first->execute(database::Query{
        "SELECT * FROM users WHERE id = ?",
        {model::AttributeValue{Int64{1}}}
    });
    assert(calls.size() == 2);

    {
        auto transaction = manager.transaction();
        assert(transaction.active());
        transaction.connection().execute("UPDATE users SET active = TRUE");
        transaction.commit();
        assert(!transaction.active());
    }

    assert(began == 1);
    assert(committed == 1);
    assert(rolled_back == 0);

    {
        auto transaction = manager.transaction();
        assert(transaction.active());
    }

    assert(began == 2);
    assert(rolled_back == 1);

    database::runtime::use(manager);
    assert(database::runtime::configured());
    assert(database::runtime::connection()->name() == "default");
    database::runtime::clear();
    assert(!database::runtime::configured());

    return 0;
}
