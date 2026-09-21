#include <cassert>
#include <optional>
#include <string_view>

#include <gungnir/model/model.hpp>

class Post;

class User : public gungnir::Model<User> {
public:
    inline static constexpr gungnir::Table table{"users"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"name", "email", "active", "nickname"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> name;
    gungnir::Field<gungnir::String> email;
    gungnir::Field<gungnir::Boolean> active;
    gungnir::Field<std::optional<gungnir::String>> nickname;

    gungnir::HasMany<Post> posts{"user_id"};
};

class Post : public gungnir::Model<Post> {
public:
    inline static constexpr gungnir::Table table{"posts"};
    inline static constexpr gungnir::Connection connection{"reporting"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"user_id", "title"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::ForeignKey<User> user_id{"user_id"};
    gungnir::Field<gungnir::String> title;

    gungnir::BelongsTo<User> user{"user_id"};
};

// This specialization represents the output contract used by the future
// Gungnir model metadata generator. The application model itself stays clean.
template <>
struct gungnir::model::Generated<User> {
    inline static constexpr auto attributes = std::tuple{
        gungnir::model::attribute("id", &User::id),
        gungnir::model::attribute("name", &User::name),
        gungnir::model::attribute("email", &User::email),
        gungnir::model::attribute("active", &User::active),
        gungnir::model::attribute("nickname", &User::nickname)
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
    assert(User::table_name() == std::string_view{"users"});
    assert(User::connection_name() == std::string_view{"default"});
    assert(Post::connection_name() == std::string_view{"reporting"});

    assert(User::primary_key_name() == std::string_view{"id"});
    assert(User::primary_key_incrementing());

    gungnir::model::AttributeMap incoming{
        {"name", gungnir::String{"Justin"}},
        {"email", gungnir::String{"justin@example.com"}},
        {"active", true},
        {"nickname", nullptr}
    };

    User user;
    user.fill(incoming);

    assert(user.name.get() == "Justin");
    assert(user.email.get() == "justin@example.com");
    assert(user.active.get());
    assert(!user.nickname.get().has_value());
    assert(user.dirty());

    const auto dirty = user.dirty_fields();
    assert(dirty.size() == 4);
    assert(user.is_dirty("email"));

    user.clean();
    assert(!user.dirty());
    assert(user.dirty_fields().empty());

    user.email = "new@example.com";
    const auto changes = user.dirty_attributes();
    assert(changes.size() == 1);
    assert(
        std::get<gungnir::String>(changes.at("email")) ==
        "new@example.com"
    );

    bool guarded = false;
    try {
        user.fill({
            {"id", gungnir::Int64{99}}
        });
    } catch (const gungnir::MassAssignmentError&) {
        guarded = true;
    }
    assert(guarded);

    user.force_fill({
        {"id", gungnir::Int64{99}}
    });
    assert(user.id.get() == 99);

    const auto serialized = user.attributes();
    assert(serialized.contains("id"));
    assert(serialized.contains("email"));

    const auto hydrated = User::hydrate({
        {"id", gungnir::Int64{7}},
        {"name", gungnir::String{"Hydrated"}},
        {"email", gungnir::String{"db@example.com"}},
        {"active", true},
        {"nickname", gungnir::String{"H"}}
    });

    assert(hydrated.exists());
    assert(!hydrated.dirty());
    assert(hydrated.id.get() == 7);
    assert(hydrated.name.get() == "Hydrated");
    assert(hydrated.nickname.get().value() == "H");

    assert(user.relation_names().size() == 1);
    assert(!user.relation_loaded("posts"));

    user.posts.set({});
    assert(user.relation_loaded("posts"));

    user.unload_relations();
    assert(!user.relation_loaded("posts"));

    return 0;
}
