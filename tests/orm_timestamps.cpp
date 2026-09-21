#include <cassert>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include <gungnir/database/database.hpp>
#include <gungnir/orm/orm.hpp>

class TimestampedUser : public gungnir::Model<TimestampedUser> {
public:
    inline static constexpr gungnir::Table table{"users"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"name"};
    inline static constexpr bool timestamps = true;

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> name;
    gungnir::Field<std::optional<gungnir::String>> created_at;
    gungnir::Field<std::optional<gungnir::String>> updated_at;
};

template <>
struct gungnir::model::Generated<TimestampedUser> {
    inline static constexpr auto attributes = std::tuple{
        gungnir::model::attribute("id", &TimestampedUser::id),
        gungnir::model::attribute("name", &TimestampedUser::name),
        gungnir::model::attribute("created_at", &TimestampedUser::created_at),
        gungnir::model::attribute("updated_at", &TimestampedUser::updated_at)
    };

    inline static constexpr auto relations = std::tuple{};
};

int main() {
    using namespace gungnir;

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

                    if (statement.find("INSERT") != String::npos) {
                        return database::Result{
                            .rows = {},
                            .affected_rows = 1,
                            .inserted_id = Int64{7}
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

    TimestampedUser user;
    user.name = "Justin";

    assert(user.save());
    assert(user.exists());
    assert(user.id.get() == 7);
    assert(user.created_at.initialized());
    assert(user.updated_at.initialized());
    assert(user.created_at.get().has_value());
    assert(user.updated_at.get().has_value());
    assert(!user.created_at.dirty());
    assert(!user.updated_at.dirty());

    user.name = "Updated";
    assert(user.save());
    assert(!user.name.dirty());
    assert(!user.updated_at.dirty());

    assert(user.touch());
    assert(user.updated_at.get().has_value());

    database::runtime::clear();
    return 0;
}
