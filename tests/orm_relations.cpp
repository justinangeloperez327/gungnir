#include <cassert>
#include <memory>
#include <string_view>
#include <tuple>

#include <gungnir/database/database.hpp>
#include <gungnir/orm/orm.hpp>

class Post;
class Profile;
class Role;
class Comment;

class User : public gungnir::Model<User> {
public:
    inline static constexpr gungnir::Table table{
        "users"
    };

    inline static constexpr auto fillable =
        gungnir::Fillable{
            "name"
        };

    gungnir::PrimaryKey<
        gungnir::Integer
    > id;

    gungnir::Field<
        gungnir::String
    > name;

    gungnir::HasOne<Profile>
        profile{"user_id"};

    gungnir::HasMany<Post>
        posts{"user_id"};

    gungnir::BelongsToMany<Role>
        roles{
            "role_user",
            "user_id",
            "role_id"
        };

    gungnir::HasOneThrough<
        Comment,
        Post
    > first_comment{
        "user_id",
        "post_id"
    };

    gungnir::HasManyThrough<
        Comment,
        Post
    > comments{
        "user_id",
        "post_id"
    };
};

class Post : public gungnir::Model<Post> {
public:
    inline static constexpr gungnir::Table table{
        "posts"
    };

    inline static constexpr auto fillable =
        gungnir::Fillable{
            "user_id",
            "title"
        };

    gungnir::PrimaryKey<
        gungnir::Integer
    > id;

    gungnir::ForeignKey<User>
        user_id{"user_id"};

    gungnir::Field<
        gungnir::String
    > title;

    gungnir::BelongsTo<User>
        user{"user_id"};
};

class Profile : public gungnir::Model<Profile> {
public:
    inline static constexpr gungnir::Table table{
        "profiles"
    };

    inline static constexpr auto fillable =
        gungnir::Fillable{
            "user_id",
            "display_name"
        };

    gungnir::PrimaryKey<
        gungnir::Integer
    > id;

    gungnir::ForeignKey<User>
        user_id{"user_id"};

    gungnir::Field<
        gungnir::String
    > display_name;
};

class Role : public gungnir::Model<Role> {
public:
    inline static constexpr gungnir::Table table{
        "roles"
    };

    inline static constexpr auto fillable =
        gungnir::Fillable{
            "name"
        };

    gungnir::PrimaryKey<
        gungnir::Integer
    > id;

    gungnir::Field<
        gungnir::String
    > name;
};

class Comment : public gungnir::Model<Comment> {
public:
    inline static constexpr gungnir::Table table{
        "comments"
    };

    inline static constexpr auto fillable =
        gungnir::Fillable{
            "post_id",
            "body"
        };

    gungnir::PrimaryKey<
        gungnir::Integer
    > id;

    gungnir::ForeignKey<Post>
        post_id{"post_id"};

    gungnir::Field<
        gungnir::String
    > body;
};

template <>
struct gungnir::model::Generated<User> {
    inline static constexpr auto attributes =
        std::tuple{
            gungnir::model::attribute(
                "id",
                &User::id
            ),
            gungnir::model::attribute(
                "name",
                &User::name
            )
        };

    inline static constexpr auto relations =
        std::tuple{
            gungnir::model::relation(
                "profile",
                &User::profile
            ),
            gungnir::model::relation(
                "posts",
                &User::posts
            ),
            gungnir::model::relation(
                "roles",
                &User::roles
            ),
            gungnir::model::relation(
                "first_comment",
                &User::first_comment
            ),
            gungnir::model::relation(
                "comments",
                &User::comments
            )
        };
};

template <>
struct gungnir::model::Generated<Post> {
    inline static constexpr auto attributes =
        std::tuple{
            gungnir::model::attribute(
                "id",
                &Post::id
            ),
            gungnir::model::attribute(
                "user_id",
                &Post::user_id
            ),
            gungnir::model::attribute(
                "title",
                &Post::title
            )
        };

    inline static constexpr auto relations =
        std::tuple{
            gungnir::model::relation(
                "user",
                &Post::user
            )
        };
};

template <>
struct gungnir::model::Generated<Profile> {
    inline static constexpr auto attributes =
        std::tuple{
            gungnir::model::attribute(
                "id",
                &Profile::id
            ),
            gungnir::model::attribute(
                "user_id",
                &Profile::user_id
            ),
            gungnir::model::attribute(
                "display_name",
                &Profile::display_name
            )
        };

    inline static constexpr auto relations =
        std::tuple{};
};

template <>
struct gungnir::model::Generated<Role> {
    inline static constexpr auto attributes =
        std::tuple{
            gungnir::model::attribute(
                "id",
                &Role::id
            ),
            gungnir::model::attribute(
                "name",
                &Role::name
            )
        };

    inline static constexpr auto relations =
        std::tuple{};
};

template <>
struct gungnir::model::Generated<Comment> {
    inline static constexpr auto attributes =
        std::tuple{
            gungnir::model::attribute(
                "id",
                &Comment::id
            ),
            gungnir::model::attribute(
                "post_id",
                &Comment::post_id
            ),
            gungnir::model::attribute(
                "body",
                &Comment::body
            )
        };

    inline static constexpr auto relations =
        std::tuple{};
};

namespace {

bool contains(
    const gungnir::String& value,
    std::string_view text
) {
    return
        value.find(text) !=
        gungnir::String::npos;
}

} // namespace

int main() {
    using namespace gungnir;

    auto user =
        User::hydrate({
            {
                "id",
                Int64{7}
            },
            {
                "name",
                String{"Justin"}
            }
        });

    const auto posts =
        orm::relation_query(
            user,
            user.posts
        ).compile(
            database::Backend::postgresql
        );

    assert(
        contains(
            posts.text,
            "FROM \"posts\""
        )
    );

    assert(
        contains(
            posts.text,
            "WHERE \"user_id\" = $1"
        )
    );

    assert(posts.bindings.size() == 1);
    assert(
        std::get<Int64>(
            posts.bindings.front()
        ) == 7
    );

    const auto profile =
        orm::relation_query(
            user,
            user.profile
        ).compile(
            database::Backend::postgresql
        );

    assert(
        contains(
            profile.text,
            "FROM \"profiles\""
        )
    );

    assert(
        contains(
            profile.text,
            "WHERE \"user_id\" = $1"
        )
    );

    auto post =
        Post::hydrate({
            {
                "id",
                Int64{11}
            },
            {
                "user_id",
                Int64{7}
            },
            {
                "title",
                String{"Post"}
            }
        });

    const auto owner =
        orm::relation_query(
            post,
            post.user
        ).compile(
            database::Backend::postgresql
        );

    assert(
        contains(
            owner.text,
            "FROM \"users\""
        )
    );

    assert(
        contains(
            owner.text,
            "WHERE \"id\" = $1"
        )
    );

    const auto roles =
        orm::relation_query(
            user,
            user.roles
        ).compile(
            database::Backend::postgresql
        );

    assert(
        contains(
            roles.text,
            "SELECT \"roles\".*"
        )
    );

    assert(
        contains(
            roles.text,
            "INNER JOIN \"role_user\""
        )
    );

    assert(
        contains(
            roles.text,
            "\"roles\".\"id\" = "
            "\"role_user\".\"role_id\""
        )
    );

    assert(
        contains(
            roles.text,
            "WHERE "
            "\"role_user\".\"user_id\" = $1"
        )
    );

    const auto comments =
        orm::relation_query(
            user,
            user.comments
        ).compile(
            database::Backend::postgresql
        );

    assert(
        contains(
            comments.text,
            "SELECT \"comments\".*"
        )
    );

    assert(
        contains(
            comments.text,
            "INNER JOIN \"posts\""
        )
    );

    assert(
        contains(
            comments.text,
            "\"comments\".\"post_id\" = "
            "\"posts\".\"id\""
        )
    );

    assert(
        contains(
            comments.text,
            "WHERE \"posts\".\"user_id\" = $1"
        )
    );

    const auto first_comment =
        orm::relation_query(
            user,
            user.first_comment
        ).compile(
            database::Backend::postgresql
        );

    assert(
        contains(
            first_comment.text,
            "SELECT \"comments\".*"
        )
    );

    User unsaved;

    const auto empty =
        orm::relation_query(
            unsaved,
            unsaved.posts
        ).compile(
            database::Backend::postgresql
        );

    assert(
        contains(
            empty.text,
            "WHERE 1 = 0"
        )
    );

    assert(empty.bindings.empty());

    const auto mongo =
        orm::relation_query(
            user,
            user.posts
        ).compile(
            database::Backend::mongodb
        );

    assert(
        mongo.kind ==
        orm::CompiledQueryKind::mongodb
    );

    assert(
        contains(
            mongo.text,
            "\"user_id\":{\"$eq\":"
        )
    );

    assert(mongo.bindings.size() == 1);
    assert(
        std::get<Int64>(
            mongo.bindings.front()
        ) == 7
    );

    database::Manager manager;

    manager.add(
        "default",
        database::Backend::postgresql,
        [] {
            return std::make_shared<
                database::CallbackDriver
            >(
                database::Backend::postgresql,
                [](
                    const String& statement,
                    const auto& bindings
                ) {
                    assert(
                        contains(
                            statement,
                            "FROM \"posts\""
                        )
                    );

                    assert(
                        contains(
                            statement,
                            "WHERE \"user_id\" = $1"
                        )
                    );

                    assert(
                        bindings.size() == 1
                    );

                    assert(
                        std::get<Int64>(
                            bindings.front()
                        ) == 7
                    );

                    return database::Result{
                        .rows = {
                            {
                                {
                                    "id",
                                    Int64{11}
                                },
                                {
                                    "user_id",
                                    Int64{7}
                                },
                                {
                                    "title",
                                    String{"Scoped"}
                                }
                            }
                        },
                        .affected_rows = 1,
                        .inserted_id = {}
                    };
                }
            );
        }
    );

    database::runtime::use(
        manager
    );

    const auto scoped =
        orm::relation_query(
            user,
            user.posts
        ).get();

    assert(scoped.size() == 1);
    assert(
        scoped.first().user_id.get() ==
        7
    );

    assert(
        scoped.first().title.get() ==
        "Scoped"
    );

    database::runtime::clear();

    return 0;
}
