#include <cassert>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

#include <gungnir/database/database.hpp>
#include <gungnir/orm/orm.hpp>

class User : public gungnir::Model<User> {
public:
    inline static constexpr gungnir::Table table{"users"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"name"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> name;
};

template <>
struct gungnir::model::Generated<User> {
    inline static constexpr auto attributes = std::tuple{
        gungnir::model::attribute("id", &User::id),
        gungnir::model::attribute("name", &User::name)
    };

    inline static constexpr auto relations = std::tuple{};
};

int main() {
    using namespace gungnir;

    int next_driver = 0;
    int began_on = -1;
    int committed = 0;
    int rolled_back = 0;
    std::vector<int> executed_on;

    database::Manager manager;
    manager.add(
        "default",
        database::Backend::postgresql,
        [&] {
            const int driver_id = next_driver++;

            return std::make_shared<database::CallbackDriver>(
                database::Backend::postgresql,
                [&, driver_id](const String&, const auto&) {
                    executed_on.push_back(driver_id);
                    return database::Result{
                        .rows = {{
                            {"id", Int64{1}},
                            {"name", String{"Scoped"}}
                        }},
                        .affected_rows = 1,
                        .inserted_id = {}
                    };
                },
                [&, driver_id] {
                    began_on = driver_id;
                },
                [&] {
                    ++committed;
                },
                [&] {
                    ++rolled_back;
                }
            );
        },
        2
    );

    database::runtime::use(manager);

    {
        auto transaction = manager.transaction();
        const auto result = transaction.run([] {
            return User::query()
                .lock_for_update()
                .first_or_fail();
        });

        assert(result.id.get() == 1);
        assert(!executed_on.empty());
        assert(executed_on.back() == began_on);
        assert(committed == 1);
        assert(!transaction.active());
    }

    bool threw = false;
    try {
        auto transaction = manager.transaction();
        transaction.run([] {
            static_cast<void>(User::query().first());
            throw std::runtime_error("rollback");
        });
    } catch (const std::runtime_error&) {
        threw = true;
    }

    assert(threw);
    assert(rolled_back == 1);

    const auto postgres_lock = User::query()
        .lock_for_update()
        .compile(database::Backend::postgresql);
    assert(
        postgres_lock.text.find("FOR UPDATE") !=
        String::npos
    );

    const auto mysql_lock = User::query()
        .shared_lock()
        .compile(database::Backend::mysql);
    assert(
        mysql_lock.text.find("FOR SHARE") !=
        String::npos
    );

    const auto mssql_lock = User::query()
        .lock_for_update()
        .compile(database::Backend::mssql);
    assert(
        mssql_lock.text.find("WITH (UPDLOCK, ROWLOCK)") !=
        String::npos
    );

    bool mongo_rejected = false;
    try {
        static_cast<void>(
            User::query()
                .lock_for_update()
                .compile(database::Backend::mongodb)
        );
    } catch (const std::logic_error&) {
        mongo_rejected = true;
    }
    assert(mongo_rejected);

    database::runtime::clear();
    return 0;
}
