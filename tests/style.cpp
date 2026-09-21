#include <cassert>
#include <string_view>

#include <gungnir/controller/controller.hpp>
#include <gungnir/migration/migration.hpp>
#include <gungnir/model/model.hpp>

class Post;

class User : public gungnir::Model<User> {
public:
    static constexpr gungnir::Table table{"users"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> name;
    gungnir::Field<gungnir::String> email;
    gungnir::HasMany<Post> posts;
};

class Post : public gungnir::Model<Post> {
public:
    static constexpr gungnir::Table table{"posts"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::ForeignKey<User> user_id;
    gungnir::Field<gungnir::String> title;
    gungnir::BelongsTo<User> user;
};

class UserController : public gungnir::Controller {
public:
    gungnir::Response index() {
        return json(R"({"users":[]})");
    }
};

class CreateUsersTable : public gungnir::Migration {
public:
    void up() override {
        Table::create("users", [](Column& column) {
            column.id();
            column.string("name");
            column.string("email").unique();
            column.boolean("active").default_value(true);
            column.timestamps();
        });
    }

    void down() override {
        Table::drop_if_exists("users");
    }
};

int main() {
    User user;
    user.id = 1;
    user.name = "Justin";

    assert(user.id.get() == 1);
    assert(user.name.get() == "Justin");
    assert(User::table.name() == std::string_view{"users"});

    Post post;
    post.user_id = user.id.get();
    assert(post.user_id.get() == 1);

    UserController controller;
    const auto response = controller.index();
    assert(response.status() == 200);
    assert(response.header("content-type") == "application/json; charset=utf-8");

    CreateUsersTable migration;

    const auto up = migration.plan_up();
    assert(up.operations().size() == 1);
    assert(up.operations()[0].type == gungnir::migration::TableOperationType::create);
    assert(up.operations()[0].name == "users");
    assert(up.operations()[0].columns.size() == 6);
    assert(up.operations()[0].columns[0].primary_value);
    assert(up.operations()[0].columns[2].unique_value);

    const auto down = migration.plan_down();
    assert(down.operations().size() == 1);
    assert(down.operations()[0].type == gungnir::migration::TableOperationType::drop_if_exists);

    return 0;
}
