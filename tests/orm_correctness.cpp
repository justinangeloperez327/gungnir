#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <vector>

#include <gungnir/database/database.hpp>
#include <gungnir/orm/orm.hpp>

class Post;

class User : public gungnir::Model<User> {
public:
    inline static constexpr
        gungnir::Table table{
            "users"
        };

    inline static constexpr auto
        fillable =
            gungnir::Fillable{
                "name",
                "nickname"
            };

    gungnir::PrimaryKey<
        gungnir::Integer
    > id;

    gungnir::Field<
        gungnir::String
    > name;

    gungnir::Field<
        std::optional<
            gungnir::String
        >
    > nickname;

    gungnir::HasMany<Post>
        posts{"user_id"};
};

class Post : public gungnir::Model<Post> {
public:
    inline static constexpr
        gungnir::Table table{
            "posts"
        };

    inline static constexpr auto
        fillable =
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
};

template <>
struct gungnir::model::Generated<User> {
    inline static constexpr auto
        attributes =
            std::tuple{
                gungnir::model::attribute(
                    "id",
                    &User::id
                ),
                gungnir::model::attribute(
                    "name",
                    &User::name
                ),
                gungnir::model::attribute(
                    "nickname",
                    &User::nickname
                )
            };

    inline static constexpr auto
        relations =
            std::tuple{
                gungnir::model::relation(
                    "posts",
                    &User::posts
                )
            };
};

template <>
struct gungnir::model::Generated<Post> {
    inline static constexpr auto
        attributes =
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

    inline static constexpr auto
        relations =
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

void hydration_is_strict_and_clean() {
    using namespace gungnir;

    auto user =
        User::hydrate({
            {
                "id",
                Int64{7}
            },
            {
                "name",
                String{"Hydrated"}
            },
            {
                "nickname",
                nullptr
            }
        });

    assert(user.exists());
    assert(!user.dirty());
    assert(user.id.get() == 7);
    assert(
        user.name.get() ==
        "Hydrated"
    );
    assert(
        !user.nickname.get()
            .has_value()
    );

    bool bad_type = false;

    try {
        static_cast<void>(
            User::hydrate({
                {
                    "id",
                    Int64{8}
                },
                {
                    "name",
                    Int64{123}
                }
            })
        );
    } catch (
        const std::invalid_argument&
    ) {
        bad_type = true;
    }

    assert(bad_type);

    bool out_of_range = false;

    try {
        static_cast<void>(
            model::value_cast<
                std::int8_t
            >(
                model::AttributeValue{
                    Int64{200}
                }
            )
        );
    } catch (
        const std::out_of_range&
    ) {
        out_of_range = true;
    }

    assert(out_of_range);

    bool negative_unsigned = false;

    try {
        static_cast<void>(
            model::value_cast<
                std::uint8_t
            >(
                model::AttributeValue{
                    Int64{-1}
                }
            )
        );
    } catch (
        const std::invalid_argument&
    ) {
        negative_unsigned = true;
    }

    assert(negative_unsigned);
}

void persistence_and_eager_loading_are_deterministic() {
    using namespace gungnir;

    std::vector<String> statements;
    std::atomic_bool fail_update{
        false
    };

    database::Manager manager;

    manager.add(
        "default",
        database::Backend::postgresql,
        [&] {
            return
                std::make_shared<
                    database::
                        CallbackDriver
                >(
                    database::
                        Backend::
                            postgresql,
                    [&](
                        const String&
                            statement,
                        const std::vector<
                            model::
                                AttributeValue
                        >&
                    ) {
                        statements
                            .push_back(
                                statement
                            );

                        if (
                            contains(
                                statement,
                                "INSERT INTO"
                            )
                        ) {
                            return
                                database::Result{
                                    .rows = {},
                                    .affected_rows =
                                        1,
                                    .inserted_id =
                                        Int64{10}
                                };
                        }

                        if (
                            contains(
                                statement,
                                "UPDATE"
                            )
                        ) {
                            return
                                database::Result{
                                    .rows = {},
                                    .affected_rows =
                                        fail_update
                                            .load()
                                            ? 0u
                                            : 1u,
                                    .inserted_id =
                                        {}
                                };
                        }

                        if (
                            contains(
                                statement,
                                "FROM \"users\""
                            )
                        ) {
                            return
                                database::Result{
                                    .rows = {
                                        {
                                            {
                                                "id",
                                                Int64{1}
                                            },
                                            {
                                                "name",
                                                String{
                                                    "One"
                                                }
                                            },
                                            {
                                                "nickname",
                                                nullptr
                                            }
                                        },
                                        {
                                            {
                                                "id",
                                                Int64{2}
                                            },
                                            {
                                                "name",
                                                String{
                                                    "Two"
                                                }
                                            },
                                            {
                                                "nickname",
                                                String{
                                                    "T"
                                                }
                                            }
                                        }
                                    },
                                    .affected_rows =
                                        2,
                                    .inserted_id =
                                        {}
                                };
                        }

                        if (
                            contains(
                                statement,
                                "FROM \"posts\""
                            )
                        ) {
                            return
                                database::Result{
                                    .rows = {
                                        {
                                            {
                                                "id",
                                                Int64{11}
                                            },
                                            {
                                                "user_id",
                                                Int64{1}
                                            },
                                            {
                                                "title",
                                                String{
                                                    "A"
                                                }
                                            }
                                        },
                                        {
                                            {
                                                "id",
                                                Int64{12}
                                            },
                                            {
                                                "user_id",
                                                Int64{1}
                                            },
                                            {
                                                "title",
                                                String{
                                                    "B"
                                                }
                                            }
                                        },
                                        {
                                            {
                                                "id",
                                                Int64{13}
                                            },
                                            {
                                                "user_id",
                                                Int64{2}
                                            },
                                            {
                                                "title",
                                                String{
                                                    "C"
                                                }
                                            }
                                        }
                                    },
                                    .affected_rows =
                                        3,
                                    .inserted_id =
                                        {}
                                };
                        }

                        return
                            database::Result{
                                .rows = {},
                                .affected_rows = 1,
                                .inserted_id = {}
                            };
                    },
                    database::
                        CallbackDriver::
                            Action{},
                    database::
                        CallbackDriver::
                            Action{},
                    database::
                        CallbackDriver::
                            Action{},
                    [] {
                        return true;
                    }
                );
        },
        database::PoolOptions{
            .size = 1,
            .acquire_timeout =
                std::chrono::
                    milliseconds{200},
            .validation_interval =
                std::chrono::
                    seconds{30},
            .reconnect_attempts = 1
        }
    );

    database::runtime::use(
        manager
    );

    {
        User user;

        user.fill({
            {
                "name",
                String{"Created"}
            },
            {
                "nickname",
                nullptr
            }
        });

        assert(user.dirty());
        assert(user.save());
        assert(user.exists());
        assert(
            user.was_recently_created()
        );
        assert(user.id.get() == 10);
        assert(!user.dirty());

        user.name = "Updated";

        assert(
            user.is_dirty("name")
        );
        assert(user.save());
        assert(!user.dirty());
        assert(
            !user.was_recently_created()
        );

        user.name = "Rejected";
        fail_update.store(true);

        assert(!user.save());
        assert(user.dirty());
        assert(
            user.is_dirty("name")
        );

        fail_update.store(false);
    }

    assert(
        manager.pool_stats()
            .leased == 0
    );

    statements.clear();

    std::size_t query_events = 0;

    orm::listen(
        [&](
            const orm::QueryEvent&
        ) {
            ++query_events;
        }
    );

    const auto users =
        User::with("posts")
            .order_by("id")
            .get();

    orm::stop_listening();

    assert(users.size() == 2);
    assert(query_events == 2);
    assert(statements.size() == 2);

    assert(
        contains(
            statements[0],
            "FROM \"users\""
        )
    );

    assert(
        contains(
            statements[1],
            "FROM \"posts\""
        )
    );

    assert(
        contains(
            statements[1],
            "\"user_id\" IN"
        )
    );

    assert(
        users[0].posts.loaded()
    );
    assert(
        users[1].posts.loaded()
    );
    assert(
        users[0].posts.size() == 2
    );
    assert(
        users[1].posts.size() == 1
    );

    assert(
        users[0].posts
            .first()
            .title.get() ==
        "A"
    );

    assert(
        users[1].posts
            .first()
            .title.get() ==
        "C"
    );

    const auto stats =
        manager.pool_stats();

    assert(stats.leased == 0);
    assert(stats.available == 1);

    database::runtime::clear();
}

} // namespace

int main() {
    hydration_is_strict_and_clean();
    persistence_and_eager_loading_are_deterministic();
}
