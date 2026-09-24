#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gungnir/database/database.hpp>
#include <gungnir/database/mongodb.hpp>
#include <gungnir/migration/migration.hpp>
#include <gungnir/migration/runner.hpp>
#include <gungnir/orm/orm.hpp>

class MongoUser : public gungnir::Model<MongoUser> {
public:
    inline static constexpr gungnir::Table table{
        "gungnir_mongodb_users"
    };

    inline static constexpr auto fillable =
        gungnir::Fillable{
            "name",
            "active"
        };

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> name;
    gungnir::Field<gungnir::Boolean> active;
};

template <>
struct gungnir::model::Generated<MongoUser> {
    inline static constexpr auto attributes =
        std::tuple{
            gungnir::model::attribute(
                "id",
                &MongoUser::id
            ),
            gungnir::model::attribute(
                "name",
                &MongoUser::name
            ),
            gungnir::model::attribute(
                "active",
                &MongoUser::active
            )
        };

    inline static constexpr auto relations =
        std::tuple{};
};

class CreateMongoUsers final :
    public gungnir::Migration {
public:
    void up() override {
        Table::create(
            "gungnir_mongodb_users",
            [](Column& column) {
                column.id();
                column.string("name");
                column.boolean("active");
            }
        );
    }

    void down() override {
        Table::drop_if_exists(
            "gungnir_mongodb_users"
        );
    }
};

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
                "GUNGNIR_MONGODB_PORT",
                "27017"
            )
        )
    );
}

void drop_if_present(
    gungnir::database::Connection& connection,
    const gungnir::String& collection
) {
    try {
        connection.execute(
            "{\"drop\":\"" +
            collection +
            "\"}"
        );
    } catch (
        const gungnir::database::Error&
    ) {
    }
}

} // namespace

int main() {
    using namespace gungnir;

    database::Settings settings;
    settings.name = "default";
    settings.backend =
        database::Backend::mongodb;
    settings.host =
        env(
            "GUNGNIR_MONGODB_HOST",
            "127.0.0.1"
        );
    settings.port = port();
    settings.database =
        env(
            "GUNGNIR_MONGODB_DATABASE",
            "gungnir"
        );
    settings.username =
        env(
            "GUNGNIR_MONGODB_USERNAME",
            ""
        );
    settings.password =
        env(
            "GUNGNIR_MONGODB_PASSWORD",
            ""
        );
    settings.options =
        env(
            "GUNGNIR_MONGODB_OPTIONS",
            ""
        );

    database::DriverRegistry registry;
    database::register_mongodb(
        registry
    );

    assert(
        registry.has(
            database::Backend::mongodb
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
    assert(!connection->supports_transactions());
    assert(!connection->supports_savepoints());

    drop_if_present(
        *connection,
        "gungnir_mongodb_users"
    );
    drop_if_present(
        *connection,
        "gungnir_migrations"
    );
    drop_if_present(
        *connection,
        "gungnir_sequences"
    );

    database::runtime::use(manager);

    CreateMongoUsers migration;
    migration::DatabaseRepository repository;
    migration::Runner runner{
        repository
    };

    std::vector<migration::Named> migrations{
        {
            "2026_09_24_000001_create_mongo_users",
            &migration
        }
    };

    assert(
        runner.migrate(
            migrations
        ) == 1
    );

    assert(
        runner.migrate(
            migrations
        ) == 0
    );

    const auto status =
        runner.status(
            migrations
        );

    assert(status.size() == 1);
    assert(status.front().applied);
    assert(status.front().batch == 1);

    auto created =
        MongoUser::create({
            {
                "name",
                String{"Mongo User"}
            },
            {
                "active",
                Boolean{true}
            }
        });

    assert(created.exists());
    assert(created.id.initialized());
    assert(created.id.get() > 0);

    const auto id =
        created.id.get();

    const auto found =
        MongoUser::find(id);

    assert(found.has_value());
    assert(found->id.get() == id);
    assert(
        found->name.get() ==
        "Mongo User"
    );
    assert(found->active.get());

    created.name =
        "Updated Mongo User";

    assert(created.save());

    const auto updated =
        MongoUser::find(id);

    assert(updated.has_value());
    assert(
        updated->name.get() ==
        "Updated Mongo User"
    );

    assert(
        MongoUser::where(
            "active",
            true
        ).count() == 1
    );

    assert(created.remove());
    assert(
        !MongoUser::find(id)
            .has_value()
    );

    assert(
        runner.rollback(
            migrations
        ) == 1
    );

    const auto rolled_back =
        runner.status(
            migrations
        );

    assert(
        rolled_back.size() == 1
    );
    assert(
        !rolled_back.front().applied
    );

    drop_if_present(
        *connection,
        "gungnir_migrations"
    );
    drop_if_present(
        *connection,
        "gungnir_sequences"
    );

    database::runtime::clear();
    return 0;
}
