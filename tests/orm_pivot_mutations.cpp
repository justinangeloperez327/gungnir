#include <algorithm>
#include <cassert>
#include <memory>
#include <string_view>
#include <tuple>
#include <vector>

#include <gungnir/database/database.hpp>
#include <gungnir/orm/orm.hpp>

class Role;

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

    gungnir::BelongsToMany<Role>
        roles{
            "role_user",
            "user_id",
            "role_id"
        };
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
                "roles",
                &User::roles
            )
        };
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

namespace {

bool contains(
    const gungnir::String& value,
    std::string_view text
) {
    return
        value.find(text) !=
        gungnir::String::npos;
}

bool contains_value(
    const std::vector<
        gungnir::model::AttributeValue
    >& values,
    gungnir::Int64 expected
) {
    return
        std::find(
            values.begin(),
            values.end(),
            gungnir::model::AttributeValue{
                expected
            }
        ) !=
        values.end();
}

} // namespace

int main() {
    using namespace gungnir;

    std::vector<String> statements;
    std::vector<std::size_t>
        binding_counts;

    int begins = 0;
    int commits = 0;
    int rollbacks = 0;

    database::Manager manager;

    manager.add(
        "default",
        database::Backend::postgresql,
        [&] {
            return std::make_shared<
                database::CallbackDriver
            >(
                database::Backend::postgresql,
                [&](
                    const String& statement,
                    const std::vector<
                        model::AttributeValue
                    >& bindings
                ) {
                    statements.push_back(
                        statement
                    );

                    binding_counts.push_back(
                        bindings.size()
                    );

                    if (
                        contains(
                            statement,
                            "SELECT \"role_id\" "
                            "FROM \"role_user\""
                        )
                    ) {
                        assert(
                            bindings.size() ==
                            1
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
                                        "role_id",
                                        Int64{2}
                                    }
                                },
                                {
                                    {
                                        "role_id",
                                        Int64{3}
                                    }
                                }
                            },
                            .affected_rows = 2,
                            .inserted_id = {}
                        };
                    }

                    if (
                        contains(
                            statement,
                            "INSERT INTO "
                            "\"role_user\""
                        )
                    ) {
                        return database::Result{
                            .rows = {},
                            .affected_rows =
                                bindings.size() /
                                2,
                            .inserted_id = {}
                        };
                    }

                    if (
                        contains(
                            statement,
                            "DELETE FROM "
                            "\"role_user\""
                        )
                    ) {
                        return database::Result{
                            .rows = {},
                            .affected_rows = 1,
                            .inserted_id = {}
                        };
                    }

                    return database::Result{};
                },
                [&] {
                    ++begins;
                },
                [&] {
                    ++commits;
                },
                [&] {
                    ++rollbacks;
                }
            );
        }
    );

    database::runtime::use(
        manager
    );

    std::size_t observed = 0;

    orm::listen(
        [&](const orm::QueryEvent& event) {
            ++observed;

            assert(
                event.connection ==
                "default"
            );

            assert(
                event.backend ==
                database::Backend::
                    postgresql
            );

            assert(
                !event.statement.empty()
            );
        }
    );

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

    user.roles.set({});

    assert(user.roles.loaded());

    const auto attached =
        orm::attach(
            user,
            user.roles,
            std::vector<
                model::AttributeValue
            >{
                Int64{1},
                Int64{1},
                Int64{2}
            }
        );

    assert(attached == 2);
    assert(!user.roles.loaded());

    assert(
        contains(
            statements.back(),
            "INSERT INTO "
            "\"role_user\""
        )
    );

    assert(
        binding_counts.back() ==
        4
    );

    user.roles.set({});

    const auto detached =
        orm::detach(
            user,
            user.roles,
            Int64{1}
        );

    assert(detached == 1);
    assert(!user.roles.loaded());

    assert(
        contains(
            statements.back(),
            "DELETE FROM "
            "\"role_user\""
        )
    );

    assert(
        contains(
            statements.back(),
            "\"user_id\" = $1"
        )
    );

    assert(
        contains(
            statements.back(),
            "\"role_id\" IN ($2)"
        )
    );

    user.roles.set({});

    const auto changes =
        orm::sync(
            user,
            user.roles,
            std::vector<
                model::AttributeValue
            >{
                Int64{3},
                Int64{4},
                Int64{4}
            }
        );

    assert(changes.changed());
    assert(
        changes.attached.size() ==
        1
    );
    assert(
        changes.detached.size() ==
        1
    );

    assert(
        contains_value(
            changes.attached,
            4
        )
    );

    assert(
        contains_value(
            changes.detached,
            2
        )
    );

    assert(!user.roles.loaded());
    assert(begins == 1);
    assert(commits == 1);
    assert(rollbacks == 0);

    const auto all_detached =
        orm::detach(
            user,
            user.roles
        );

    assert(all_detached == 1);

    assert(
        contains(
            statements.back(),
            "DELETE FROM "
            "\"role_user\" "
            "WHERE \"user_id\" = $1"
        )
    );

    assert(
        !contains(
            statements.back(),
            "\"role_id\" IN"
        )
    );

    User unsaved;

    bool rejected = false;

    try {
        static_cast<void>(
            orm::attach(
                unsaved,
                unsaved.roles,
                Int64{9}
            )
        );
    } catch (
        const orm::QueryError&
    ) {
        rejected = true;
    }

    assert(rejected);

    assert(
        observed ==
        statements.size()
    );

    orm::stop_listening();
    database::runtime::clear();

    return 0;
}
