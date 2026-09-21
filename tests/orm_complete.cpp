#include <cassert>
#include <memory>
#include <string_view>
#include <tuple>
#include <vector>

#include <gungnir/database/database.hpp>
#include <gungnir/orm/orm.hpp>
#include <gungnir/orm/model_helpers.hpp>

class User : public gungnir::Model<User> {
public:
    inline static constexpr gungnir::Table table{"users"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"name", "email", "age", "score", "active"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> name;
    gungnir::Field<gungnir::String> email;
    gungnir::Field<gungnir::Integer> age;
    gungnir::Field<gungnir::Double> score;
    gungnir::Field<gungnir::Boolean> active;
};

template <>
struct gungnir::model::Generated<User> {
    inline static constexpr auto attributes = std::tuple{
        gungnir::model::attribute("id", &User::id),
        gungnir::model::attribute("name", &User::name),
        gungnir::model::attribute("email", &User::email),
        gungnir::model::attribute("age", &User::age),
        gungnir::model::attribute("score", &User::score),
        gungnir::model::attribute("active", &User::active)
    };

    inline static constexpr auto relations = std::tuple{};
};

bool contains(const gungnir::String& value, std::string_view text) {
    return value.find(text) != gungnir::String::npos;
}

int main() {
    using namespace gungnir;

    auto compiled = User::query()
        .select({"users.id", "users.name"})
        .distinct()
        .left_join(
            "profiles",
            "users.id",
            orm::Comparison::equal,
            "profiles.user_id"
        )
        .where_between("users.age", Int64{18}, Int64{65})
        .where_column(
            "users.id",
            orm::Comparison::equal,
            "profiles.user_id"
        )
        .group_by({"users.id", "users.name"})
        .having(
            "users.id",
            orm::Comparison::greater_than,
            Int64{0}
        )
        .order_by_desc("users.name")
        .compile(database::Backend::postgresql);

    assert(contains(compiled.text, "SELECT DISTINCT"));
    assert(contains(compiled.text, "LEFT JOIN \"profiles\""));
    assert(contains(compiled.text, "\"users\".\"age\" BETWEEN $1 AND $2"));
    assert(contains(compiled.text, "\"users\".\"id\" = \"profiles\".\"user_id\""));
    assert(contains(compiled.text, "GROUP BY"));
    assert(contains(compiled.text, "HAVING"));
    assert(contains(compiled.text, "ORDER BY \"users\".\"name\" DESC"));
    assert(compiled.bindings.size() == 3);

    bool condition = true;
    auto scoped = User::query()
        .when(condition, [](auto& query) {
            query.where("active", true);
        })
        .scope([](auto& query) {
            query.where_not_null("email");
        })
        .take(10)
        .skip(5);

    assert(scoped.plan().predicates.size() == 2);
    assert(scoped.plan().limit == 10);
    assert(scoped.plan().offset == 5);

    const auto mongo = User::query()
        .where("active", true)
        .or_where("age", orm::Comparison::greater_than, Int64{21})
        .compile(database::Backend::mongodb);

    assert(contains(mongo.text, "\"$or\""));
    assert(mongo.bindings.size() == 2);

    std::vector<String> statements;
    std::size_t select_count = 0;

    database::Manager manager;
    manager.add(
        "default",
        database::Backend::postgresql,
        [&] {
            return std::make_shared<database::CallbackDriver>(
                database::Backend::postgresql,
                [&](const String& statement, const auto&) {
                    statements.push_back(statement);

                    if (contains(statement, "COUNT(*)")) {
                        return database::Result{
                            .rows = {{{"aggregate", Int64{3}}}},
                            .affected_rows = 0,
                            .inserted_id = {}
                        };
                    }

                    if (contains(statement, "SUM(")) {
                        return database::Result{
                            .rows = {{{"aggregate", Double{30.5}}}},
                            .affected_rows = 0,
                            .inserted_id = {}
                        };
                    }

                    if (contains(statement, "AVG(")) {
                        return database::Result{
                            .rows = {{{"aggregate", Double{10.0}}}},
                            .affected_rows = 0,
                            .inserted_id = {}
                        };
                    }

                    if (contains(statement, "MIN(")) {
                        return database::Result{
                            .rows = {{{"aggregate", Int64{18}}}},
                            .affected_rows = 0,
                            .inserted_id = {}
                        };
                    }

                    if (contains(statement, "MAX(")) {
                        return database::Result{
                            .rows = {{{"aggregate", Int64{65}}}},
                            .affected_rows = 0,
                            .inserted_id = {}
                        };
                    }

                    if (contains(statement, "SELECT \"email\"")) {
                        return database::Result{
                            .rows = {
                                {{"email", String{"a@example.com"}}},
                                {{"email", String{"b@example.com"}}}
                            },
                            .affected_rows = 2,
                            .inserted_id = {}
                        };
                    }

                    if (contains(statement, "SELECT")) {
                        ++select_count;
                        return database::Result{
                            .rows = {
                                {
                                    {"id", Int64{1}},
                                    {"name", String{"A"}},
                                    {"email", String{"a@example.com"}},
                                    {"age", Int64{21}},
                                    {"score", Double{10.0}},
                                    {"active", true}
                                },
                                {
                                    {"id", Int64{2}},
                                    {"name", String{"B"}},
                                    {"email", String{"b@example.com"}},
                                    {"age", Int64{30}},
                                    {"score", Double{20.5}},
                                    {"active", true}
                                }
                            },
                            .affected_rows = 2,
                            .inserted_id = {}
                        };
                    }

                    if (contains(statement, "INSERT")) {
                        return database::Result{
                            .rows = {},
                            .affected_rows = 2,
                            .inserted_id = Int64{10}
                        };
                    }

                    return database::Result{
                        .rows = {},
                        .affected_rows = 4,
                        .inserted_id = {}
                    };
                }
            );
        }
    );

    database::runtime::use(manager);

    assert(User::query().count() == 3);
    assert(User::query().exists());
    assert(User::query().sum("score") == 30.5);
    assert(User::query().average("score") == 10.0);
    assert(std::get<Int64>(*User::query().min("age")) == 18);
    assert(std::get<Int64>(*User::query().max("age")) == 65);

    const auto emails = User::query().pluck("email");
    assert(emails.size() == 2);

    const auto one_email = User::query().value("email");
    assert(one_email.has_value());

    const auto page = User::query().paginate(2, 2);
    assert(page.current_page == 2);
    assert(page.per_page == 2);
    assert(page.total == 3);
    assert(page.last_page == 2);
    assert(page.data.size() == 2);

    assert(
        User::where("active", true)
            .update({{"active", false}}) == 4
    );

    assert(
        User::where("active", false)
            .increment("score", 2.5) == 4
    );

    assert(
        User::where("active", false)
            .decrement("score", 1.0) == 4
    );

    assert(
        User::where("active", false)
            .remove() == 4
    );

    assert(
        User::query().insert_many({
            {
                {"name", String{"C"}},
                {"email", String{"c@example.com"}},
                {"age", Int64{20}},
                {"score", Double{1.0}},
                {"active", true}
            },
            {
                {"name", String{"D"}},
                {"email", String{"d@example.com"}},
                {"age", Int64{22}},
                {"score", Double{2.0}},
                {"active", true}
            }
        }) == 2
    );

    int chunks = 0;
    const auto finished = User::query()
        .limit(5)
        .chunk(2, [&](auto& models) {
            ++chunks;
            assert(models.size() == 2);
            return false;
        });
    assert(!finished);
    assert(chunks == 1);

    int each_count = 0;
    const auto each_finished = User::query()
        .each([&](auto&) {
            ++each_count;
            return false;
        }, 2);
    assert(!each_finished);
    assert(each_count == 1);

    const auto many = User::find_many({
        Int64{1},
        Int64{2}
    });
    assert(many.size() == 2);
    assert(many.count() == 2);
    assert(many.last().id.get() == 2);
    assert(many.find(Int64{1}).has_value());
    assert(many.pluck("email").size() == 2);

    const auto active_many = many.filter([](const auto& user) {
        return user.active.get();
    });
    assert(active_many.count() == 2);

    assert(User::count() == 3);
    assert(User::first().has_value());
    assert(User::first_or_fail().exists());
    assert(User::paginate(1, 2).total == 3);
    assert(
        User::where_in(
            "id",
            {Int64{1}, Int64{2}}
        ).get().count() == 2
    );
    const auto latest = User::latest().limit(1);
    assert(latest.plan().limit == 1);
    assert(!latest.plan().orders.empty());

    auto required = User::find_or_fail(Int64{1});
    assert(required.exists());
    assert(required.fresh().has_value());

    const auto replica = required.replicate();
    assert(!replica.exists());
    assert(!replica.primary_key_value().has_value());
    assert(replica.email.get() == required.email.get());

    required.name = "Changed locally";
    assert(required.is_dirty("name"));
    assert(required.refresh());
    assert(!required.is_dirty("name"));
    assert(required.name.get() == "A");

    database::runtime::clear();
    return 0;
}
