#include <cassert>
#include <variant>

#include <gungnir/migration/migration.hpp>

class CreatePostsTable : public gungnir::Migration {
public:
    void up() override {
        Table::create("posts", [](Column& column) {
            column.id();
            column.uuid("public_id").unique();
            column.foreign_id("user_id")
                .constrained("users")
                .cascade_on_delete();
            column.string("title", 180);
            column.text("body").nullable();
            column.enumeration("status", {"draft", "published"})
                .default_value("draft");
            column.decimal("price", 12, 2).default_value(0);
            column.boolean("active").default_value(true);
            column.json("metadata").nullable();
            column.timestamp("published_at").nullable();
            column.timestamps();
            column.soft_deletes();

            column.index({"user_id", "created_at"}, "posts_user_created_index");
            column.unique({"user_id", "public_id"}, "posts_user_public_unique");
        });
    }

    void down() override {
        Table::drop_if_exists("posts");
    }
};

class AlterPostsTable : public gungnir::Migration {
public:
    void up() override {
        Table::alter("posts", [](Column& column) {
            column.string("title", 220).change();
            column.string("slug", 220).unique();
            column.rename("metadata", "properties");
            column.drop("published_at");
            column.foreign("editor_id", "posts_editor_foreign")
                .references("id")
                .on("users")
                .null_on_delete();
            column.index({"status", "active"}, "posts_status_active_index");
        });

        Table::rename("posts", "articles");
    }

    void down() override {
        Table::rename("articles", "posts");

        Table::alter("posts", [](Column& column) {
            column.drop("slug");
            column.rename("properties", "metadata");
            column.timestamp("published_at").nullable();
            column.drop_foreign("posts_editor_foreign");
            column.drop_index("posts_status_active_index");
        });
    }
};

int main() {
    CreatePostsTable create;

    const auto create_plan = create.plan_up();
    assert(create_plan.operations().size() == 1);

    const auto& table = create_plan.operations().front();
    assert(table.type == gungnir::migration::TableOperationType::create);
    assert(table.name == "posts");
    assert(table.columns.size() == 13);
    assert(table.indexes.size() == 2);

    assert(table.columns[0].type == gungnir::migration::ColumnType::id);
    assert(table.columns[0].primary_value);
    assert(table.columns[0].auto_increment_value);

    assert(table.columns[2].name == "user_id");
    assert(table.columns[2].unsigned_value);
    assert(table.columns[2].reference.has_value());
    assert(table.columns[2].reference->table == "users");
    assert(
        table.columns[2].reference->on_delete ==
        gungnir::migration::ReferentialAction::cascade
    );

    assert(table.columns[5].type == gungnir::migration::ColumnType::enumeration);
    assert(table.columns[5].allowed_values.size() == 2);
    assert(std::get<gungnir::String>(table.columns[5].default_value_data) == "draft");

    assert(table.columns[6].precision == 12);
    assert(table.columns[6].scale == 2);
    assert(std::get<gungnir::Int64>(table.columns[6].default_value_data) == 0);

    assert(table.indexes[0].columns.size() == 2);
    assert(table.indexes[0].name == "posts_user_created_index");
    assert(table.indexes[1].type == gungnir::migration::IndexType::unique);

    AlterPostsTable alter;

    const auto alter_up = alter.plan_up();
    assert(alter_up.operations().size() == 2);

    const auto& changes = alter_up.operations()[0];
    assert(changes.type == gungnir::migration::TableOperationType::alter);
    assert(changes.columns.size() == 2);
    assert(changes.columns[0].change_value);
    assert(changes.commands.size() == 2);
    assert(changes.foreign_keys.size() == 1);
    assert(changes.indexes.size() == 1);
    assert(
        changes.foreign_keys[0].on_delete_action ==
        gungnir::migration::ReferentialAction::set_null
    );

    const auto& rename = alter_up.operations()[1];
    assert(rename.type == gungnir::migration::TableOperationType::rename);
    assert(rename.name == "posts");
    assert(rename.new_name == "articles");

    const auto alter_down = alter.plan_down();
    assert(alter_down.operations().size() == 2);
    assert(alter_down.operations()[1].commands.size() == 4);

    return 0;
}
