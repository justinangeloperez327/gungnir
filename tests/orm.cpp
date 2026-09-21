#include <cassert>
#include <optional>
#include <string_view>
#include <tuple>

#include <gungnir/orm/orm.hpp>

class Post;

class User : public gungnir::Model<User> {
public:
    inline static constexpr gungnir::Table table{"users"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"name", "email", "active"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> name;
    gungnir::Field<gungnir::String> email;
    gungnir::Field<gungnir::Boolean> active;

    gungnir::HasMany<Post> posts{"user_id"};
};

class Post : public gungnir::Model<Post> {
public:
    inline static constexpr gungnir::Table table{"posts"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"user_id", "title"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::ForeignKey<User> user_id{"user_id"};
    gungnir::Field<gungnir::String> title;

    gungnir::BelongsTo<User> user{"user_id"};
};

template <>
struct gungnir::model::Generated<User> {
    inline static constexpr auto attributes = std::tuple{
        gungnir::model::attribute("id", &User::id),
        gungnir::model::attribute("name", &User::name),
        gungnir::model::attribute("email", &User::email),
        gungnir::model::attribute("active", &User::active)
    };

    inline static constexpr auto relations = std::tuple{
        gungnir::model::relation("posts", &User::posts)
    };
};

template <>
struct gungnir::model::Generated<Post> {
    inline static constexpr auto attributes = std::tuple{
        gungnir::model::attribute("id", &Post::id),
        gungnir::model::attribute("user_id", &Post::user_id),
        gungnir::model::attribute("title", &Post::title)
    };

    inline static constexpr auto relations = std::tuple{
        gungnir::model::relation("user", &Post::user)
    };
};

bool contains(const gungnir::String& value, std::string_view text) {
    return value.find(text) != gungnir::String::npos;
}

int main() {
    auto query = User::query()
        .select({"id", "name", "email"})
        .where("active", true)
        .where_not_null("email")
        .order_by("name")
        .limit(25)
        .offset(50)
        .with("posts");

    assert(query.plan().table == "users");
    assert(query.plan().connection == "default");
    assert(query.plan().eager_loads.size() == 1);
    assert(query.plan().eager_loads[0] == "posts");

    const auto postgres = query.compile(
        gungnir::database::Backend::postgresql
    );
    assert(postgres.kind == gungnir::orm::CompiledQueryKind::sql);
    assert(contains(postgres.text, "SELECT \"id\", \"name\", \"email\""));
    assert(contains(postgres.text, "FROM \"users\""));
    assert(contains(postgres.text, "\"active\" = $1"));
    assert(contains(postgres.text, "\"email\" IS NOT NULL"));
    assert(contains(postgres.text, "ORDER BY \"name\" ASC"));
    assert(contains(postgres.text, "LIMIT 25 OFFSET 50"));
    assert(postgres.bindings.size() == 1);
    assert(std::get<gungnir::Boolean>(postgres.bindings[0]));

    const auto mysql = User::where(
        "email",
        gungnir::String{"test@example.com"}
    ).compile(gungnir::database::Backend::mysql);

    assert(contains(mysql.text, "FROM `users`"));
    assert(contains(mysql.text, "`email` = ?"));
    assert(mysql.bindings.size() == 1);

    const auto mssql = User::query()
        .where_in(
            "id",
            {
                gungnir::Int64{1},
                gungnir::Int64{2},
                gungnir::Int64{3}
            }
        )
        .limit(10)
        .compile(gungnir::database::Backend::mssql);

    assert(contains(mssql.text, "[id] IN (@p1, @p2, @p3)"));
    assert(contains(mssql.text, "FETCH NEXT 10 ROWS ONLY"));
    assert(mssql.bindings.size() == 3);

    const auto mongo = User::query()
        .where("active", true)
        .order_by("name", gungnir::orm::SortDirection::desc)
        .limit(5)
        .compile(gungnir::database::Backend::mongodb);

    assert(mongo.kind == gungnir::orm::CompiledQueryKind::mongodb);
    assert(contains(mongo.text, "\"find\":\"users\""));
    assert(contains(mongo.text, "\"active\":{\"$eq\":{\"$bind\":0}}"));
    assert(contains(mongo.text, "\"sort\":{\"name\":-1}"));
    assert(mongo.bindings.size() == 1);

    const auto by_id = User::query()
        .where_key(gungnir::Int64{7});

    assert(by_id.plan().limit == 1);
    assert(by_id.plan().predicates.size() == 1);
    assert(by_id.plan().predicates[0].column == "id");

    const auto hydrated = gungnir::orm::Hydrator<User>::one({
        {"id", gungnir::Int64{9}},
        {"name", gungnir::String{"Hydrated"}},
        {"email", gungnir::String{"h@example.com"}},
        {"active", true}
    });

    assert(hydrated.exists());
    assert(hydrated.id.get() == 9);
    assert(!hydrated.dirty());

    bool invalid_relation = false;
    try {
        static_cast<void>(User::with("missing"));
    } catch (const gungnir::ModelMetadataError&) {
        invalid_relation = true;
    }
    assert(invalid_relation);

    return 0;
}
