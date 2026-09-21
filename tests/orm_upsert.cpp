#include <cassert>
#include <string_view>

#include <gungnir/orm/advanced_mutation.hpp>
#include <gungnir/orm/query.hpp>

class User : public gungnir::Model<User> {
public:
    inline static constexpr gungnir::Table table{"users"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"email", "name"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> email;
    gungnir::Field<gungnir::String> name;
};

template <>
struct gungnir::model::Generated<User> {
    inline static constexpr auto attributes = std::tuple{
        gungnir::model::attribute("id", &User::id),
        gungnir::model::attribute("email", &User::email),
        gungnir::model::attribute("name", &User::name)
    };

    inline static constexpr auto relations = std::tuple{};
};

bool contains(const gungnir::String& value, std::string_view text) {
    return value.find(text) != gungnir::String::npos;
}

int main() {
    using namespace gungnir;

    const std::vector<model::AttributeMap> rows{
        {
            {"email", String{"a@example.com"}},
            {"name", String{"A"}}
        },
        {
            {"email", String{"b@example.com"}},
            {"name", String{"B"}}
        }
    };

    const auto postgres = orm::compile_upsert(
        "users",
        rows,
        {"email"},
        {"name"},
        database::Backend::postgresql,
        false
    );
    assert(contains(postgres.text, "ON CONFLICT (\"email\") DO UPDATE SET"));
    assert(contains(postgres.text, "\"name\" = EXCLUDED.\"name\""));
    assert(postgres.bindings.size() == 4);

    const auto mysql = orm::compile_upsert(
        "users",
        rows,
        {"email"},
        {"name"},
        database::Backend::mysql,
        false
    );
    assert(contains(mysql.text, "ON DUPLICATE KEY UPDATE"));
    assert(contains(mysql.text, "gungnir_new"));

    const auto mssql = orm::compile_upsert(
        "users",
        rows,
        {"email"},
        {"name"},
        database::Backend::mssql,
        false
    );
    assert(contains(mssql.text, "MERGE INTO [users] AS target"));
    assert(contains(mssql.text, "WHEN MATCHED THEN UPDATE"));
    assert(contains(mssql.text, "WHEN NOT MATCHED THEN INSERT"));

    const auto mongodb = orm::compile_upsert(
        "users",
        rows,
        {"email"},
        {"name"},
        database::Backend::mongodb,
        false
    );
    assert(contains(mongodb.text, "\"upsert\":true"));
    assert(mongodb.bindings.size() == 6);

    const auto automatic_updates = orm::compile_upsert(
        "users",
        rows,
        {"email"},
        {},
        database::Backend::postgresql,
        false
    );
    assert(contains(automatic_updates.text, "DO UPDATE SET"));

    const auto ignore = orm::compile_upsert(
        "users",
        rows,
        {"email"},
        {},
        database::Backend::postgresql,
        true
    );
    assert(contains(ignore.text, "DO NOTHING"));

    const auto mongo_ignore = orm::compile_upsert(
        "users",
        rows,
        {"email"},
        {},
        database::Backend::mongodb,
        true
    );
    assert(contains(mongo_ignore.text, "\"$setOnInsert\""));

    return 0;
}
