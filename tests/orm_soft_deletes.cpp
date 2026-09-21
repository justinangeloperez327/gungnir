#include <cassert>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <vector>

#include <gungnir/database/database.hpp>
#include <gungnir/orm/orm.hpp>

class Comment;

class Post : public gungnir::Model<Post> {
public:
    inline static constexpr gungnir::Table table{"posts"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"user_id", "title"};
    inline static constexpr gungnir::SoftDeletes soft_deletes{};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::Integer> user_id;
    gungnir::Field<gungnir::String> title;
    gungnir::Field<std::optional<gungnir::String>> deleted_at;

    gungnir::HasMany<Comment> comments{"post_id"};
};

class Comment : public gungnir::Model<Comment> {
public:
    inline static constexpr gungnir::Table table{"comments"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"post_id", "body"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::Integer> post_id;
    gungnir::Field<gungnir::String> body;
};

class User : public gungnir::Model<User> {
public:
    inline static constexpr gungnir::Table table{"users"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"name"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> name;

    gungnir::HasMany<Post> posts{"user_id"};
};

template <>
struct gungnir::model::Generated<Post> {
    inline static constexpr auto attributes = std::tuple{
        gungnir::model::attribute("id", &Post::id),
        gungnir::model::attribute("user_id", &Post::user_id),
        gungnir::model::attribute("title", &Post::title),
        gungnir::model::attribute("deleted_at", &Post::deleted_at)
    };

    inline static constexpr auto relations = std::tuple{
        gungnir::model::relation("comments", &Post::comments)
    };
};

template <>
struct gungnir::model::Generated<Comment> {
    inline static constexpr auto attributes = std::tuple{
        gungnir::model::attribute("id", &Comment::id),
        gungnir::model::attribute("post_id", &Comment::post_id),
        gungnir::model::attribute("body", &Comment::body)
    };

    inline static constexpr auto relations = std::tuple{};
};

template <>
struct gungnir::model::Generated<User> {
    inline static constexpr auto attributes = std::tuple{
        gungnir::model::attribute("id", &User::id),
        gungnir::model::attribute("name", &User::name)
    };

    inline static constexpr auto relations = std::tuple{
        gungnir::model::relation("posts", &User::posts)
    };
};

bool contains(const gungnir::String& value, std::string_view text) {
    return value.find(text) != gungnir::String::npos;
}

int main() {
    using namespace gungnir;

    const auto normal = Post::query().compile(
        database::Backend::postgresql
    );
    assert(contains(normal.text, "\"deleted_at\" IS NULL"));

    const auto with_deleted = Post::query()
        .with_deleted()
        .compile(database::Backend::postgresql);
    assert(!contains(with_deleted.text, "deleted_at"));

    const auto only_deleted = Post::query()
        .only_deleted()
        .compile(database::Backend::postgresql);
    assert(contains(only_deleted.text, "\"deleted_at\" IS NOT NULL"));

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

                    if (contains(statement, "FROM \"users\"")) {
                        return database::Result{
                            .rows = {{
                                {"id", Int64{1}},
                                {"name", String{"User"}}
                            }},
                            .affected_rows = 1,
                            .inserted_id = {}
                        };
                    }

                    if (contains(statement, "FROM \"posts\"")) {
                        return database::Result{
                            .rows = {{
                                {"id", Int64{10}},
                                {"user_id", Int64{1}},
                                {"title", String{"Post"}},
                                {"deleted_at", nullptr}
                            }},
                            .affected_rows = 1,
                            .inserted_id = {}
                        };
                    }

                    if (contains(statement, "FROM \"comments\"")) {
                        return database::Result{
                            .rows = {{
                                {"id", Int64{100}},
                                {"post_id", Int64{10}},
                                {"body", String{"Comment"}}
                            }},
                            .affected_rows = 1,
                            .inserted_id = {}
                        };
                    }

                    return database::Result{
                        .rows = {},
                        .affected_rows = 1,
                        .inserted_id = {}
                    };
                }
            );
        }
    );

    database::runtime::use(manager);

    auto users = User::with("posts.comments").get();
    assert(users.size() == 1);
    assert(users.first().posts.loaded());
    assert(users.first().posts.get().front().comments.loaded());
    assert(users.first().posts.get().front().comments.size() == 1);
    assert(statements.size() == 3);

    Post post = Post::hydrate({
        {"id", Int64{10}},
        {"user_id", Int64{1}},
        {"title", String{"Post"}},
        {"deleted_at", nullptr}
    });

    assert(!post.trashed());
    assert(post.remove());
    assert(post.trashed());
    assert(post.exists());
    assert(contains(statements.back(), "CURRENT_TIMESTAMP"));

    assert(post.restore());
    assert(!post.trashed());
    assert(post.exists());
    assert(contains(statements.back(), "SET \"deleted_at\" = NULL"));

    assert(post.force_remove());
    assert(!post.exists());
    assert(contains(statements.back(), "DELETE FROM \"posts\""));

    assert(Post::only_deleted().restore() == 1);
    assert(Post::with_deleted().force_remove() == 1);

    const auto deleted = Post::hydrate({
        {"id", Int64{11}},
        {"user_id", Int64{1}},
        {"title", String{"Deleted"}},
        {"deleted_at", String{"2026-09-21 12:00:00"}}
    });
    assert(deleted.trashed());

    database::runtime::clear();
    return 0;
}
