#include <cassert>
#include <memory>
#include <tuple>
#include <vector>

#include <gungnir/database/database.hpp>
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

int main() {
    using namespace gungnir;

    std::vector<String> statements;
    std::size_t next_id = 10;

    database::Manager manager;
    manager.add(
        "default",
        database::Backend::postgresql,
        [&] {
            return std::make_shared<database::CallbackDriver>(
                database::Backend::postgresql,
                [&](const String& statement, const auto& bindings) {
                    statements.push_back(statement);

                    if (statement.find("FROM \"users\"") != String::npos) {
                        return database::Result{
                            .rows = {
                                {
                                    {"id", Int64{1}},
                                    {"name", String{"Justin"}},
                                    {"email", String{"j@example.com"}},
                                    {"active", true}
                                },
                                {
                                    {"id", Int64{2}},
                                    {"name", String{"Ana"}},
                                    {"email", String{"a@example.com"}},
                                    {"active", true}
                                }
                            },
                            .affected_rows = 2,
                            .inserted_id = {}
                        };
                    }

                    if (statement.find("FROM \"posts\"") != String::npos) {
                        assert(bindings.size() == 2);
                        return database::Result{
                            .rows = {
                                {
                                    {"id", Int64{100}},
                                    {"user_id", Int64{1}},
                                    {"title", String{"First"}}
                                },
                                {
                                    {"id", Int64{101}},
                                    {"user_id", Int64{1}},
                                    {"title", String{"Second"}}
                                },
                                {
                                    {"id", Int64{102}},
                                    {"user_id", Int64{2}},
                                    {"title", String{"Third"}}
                                }
                            },
                            .affected_rows = 3,
                            .inserted_id = {}
                        };
                    }

                    if (statement.find("INSERT INTO \"users\"") != String::npos) {
                        return database::Result{
                            .rows = {},
                            .affected_rows = 1,
                            .inserted_id = Int64{static_cast<Int64>(next_id++)}
                        };
                    }

                    if (
                        statement.find("UPDATE \"users\"") != String::npos ||
                        statement.find("DELETE FROM \"users\"") != String::npos
                    ) {
                        return database::Result{
                            .rows = {},
                            .affected_rows = 1,
                            .inserted_id = {}
                        };
                    }

                    return database::Result{};
                }
            );
        }
    );

    database::runtime::use(manager);

    std::size_t observed_queries = 0;
    orm::listen([&](const orm::QueryEvent& event) {
        ++observed_queries;
        assert(event.connection == "default");
        assert(event.backend == database::Backend::postgresql);
        assert(!event.statement.empty());
    });

    auto eager_query = User::with("posts");
    assert(eager_query.has_eager_loads());

    auto plain_query = eager_query;
    plain_query.without_eager_loads();
    assert(!plain_query.has_eager_loads());

    auto users = eager_query.get();
    assert(users.size() == 2);
    assert(users.first().posts.loaded());
    assert(users.first().posts.size() == 2);
    assert(statements.size() == 2);

    auto created = User::create({
        {"name", String{"New"}},
        {"email", String{"new@example.com"}},
        {"active", true}
    });

    assert(created.exists());
    assert(created.was_recently_created());
    assert(created.id.get() == 10);
    assert(!created.dirty());

    created.email = "changed@example.com";
    assert(created.save());
    assert(!created.dirty());

    assert(created.update({
        {"name", String{"Updated"}}
    }));
    assert(created.name.get() == "Updated");

    assert(created.remove());
    assert(!created.exists());

    const auto found = User::find(Int64{1});
    assert(found.has_value());
    assert(found->id.get() == 1);

    assert(observed_queries >= 1);
    orm::stop_listening();
    database::runtime::clear();
    return 0;
}
