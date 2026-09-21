#include <cassert>
#include <string_view>
#include <vector>

#include <gungnir/model/model.hpp>

class Post;
class Profile;
class Role;
class Country;

class User : public gungnir::Model<User> {
public:
    inline static constexpr gungnir::Table table{"users"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"name", "email", "active"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> name;
    gungnir::Field<gungnir::String> email;
    gungnir::Field<gungnir::Boolean> active;

    gungnir::HasOne<Profile> profile{"user_id"};
    gungnir::HasMany<Post> posts{"user_id"};
    gungnir::BelongsToMany<Role> roles{
        "role_user",
        "user_id",
        "role_id"
    };
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

class Profile : public gungnir::Model<Profile> {
public:
    inline static constexpr gungnir::Table table{"profiles"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"user_id", "bio"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::ForeignKey<User> user_id{"user_id"};
    gungnir::Field<gungnir::String> bio;

    gungnir::BelongsTo<User> user{"user_id"};
};

class Role : public gungnir::Model<Role> {
public:
    inline static constexpr gungnir::Table table{"roles"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"name"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> name;
};

class Country : public gungnir::Model<Country> {
public:
    inline static constexpr gungnir::Table table{"countries"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"name"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> name;
};

class Company : public gungnir::Model<Company> {
public:
    inline static constexpr gungnir::Table table{"companies"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"country_id", "name"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::ForeignKey<Country> country_id{"country_id"};
    gungnir::Field<gungnir::String> name;
};

class Employee : public gungnir::Model<Employee> {
public:
    inline static constexpr gungnir::Table table{"employees"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"company_id", "name"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::ForeignKey<Company> company_id{"company_id"};
    gungnir::Field<gungnir::String> name;
};

class ExtendedCountry : public gungnir::Model<ExtendedCountry> {
public:
    inline static constexpr gungnir::Table table{"countries"};
    inline static constexpr auto fillable = gungnir::Fillable{"name"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> name;

    gungnir::HasOneThrough<Employee, Company> manager{
        "country_id",
        "company_id"
    };

    gungnir::HasManyThrough<Employee, Company> employees{
        "country_id",
        "company_id"
    };
};

int main() {
    static_assert(User::table_name() == std::string_view{"users"});
    static_assert(User::is_fillable("name"));
    static_assert(User::is_fillable("email"));
    static_assert(!User::is_fillable("id"));
    static_assert(User::fillable_fields().size() == 3);

    User user;
    assert(!user.exists());
    assert(!user.was_recently_created());

    user.mark_persisted(true);
    assert(user.exists());
    assert(user.was_recently_created());

    user.clear_recently_created();
    assert(!user.was_recently_created());

    user.id = 1;
    user.name = "Justin";
    user.email = "justin@example.com";
    user.active = true;

    assert(user.id.name() == "id");
    assert(user.id.incrementing());

    user.name.sync_original();
    assert(!user.name.dirty());

    user.name = "Justin A.";
    assert(user.name.dirty());
    assert(user.name.original() == "Justin");

    user.name.reset();
    assert(user.name.get() == "Justin");
    assert(!user.name.dirty());

    Post post;
    post.user_id = 1;

    assert(post.user_id.name() == "user_id");
    assert(post.user_id.related_key() == "id");

    assert(user.posts.foreign_key() == "user_id");
    assert(user.posts.local_key() == "id");

    bool threw = false;
    try {
        static_cast<void>(user.posts.get());
    } catch (const gungnir::RelationNotLoaded&) {
        threw = true;
    }
    assert(threw);

    std::vector<Post> posts;
    posts.emplace_back();
    posts.emplace_back();
    user.posts.set(std::move(posts));

    assert(user.posts.loaded());
    assert(user.posts.size() == 2);

    assert(post.user.foreign_key() == "user_id");
    assert(post.user.owner_key() == "id");

    User owner;
    owner.id = 1;
    post.user.set(std::move(owner));
    assert(post.user.loaded());
    assert(post.user.get().id.get() == 1);

    assert(user.roles.pivot_table() == "role_user");
    assert(user.roles.foreign_pivot_key() == "user_id");
    assert(user.roles.related_pivot_key() == "role_id");

    ExtendedCountry country;
    assert(country.manager.first_key() == "country_id");
    assert(country.manager.second_key() == "company_id");
    assert(country.employees.first_key() == "country_id");
    assert(country.employees.second_key() == "company_id");

    return 0;
}
