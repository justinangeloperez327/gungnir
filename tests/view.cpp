#include <cassert>
#include <optional>
#include <string>
#include <tuple>

#include <gungnir/gungnir.hpp>
#include <gungnir/orm/orm.hpp>

#ifndef GUNGNIR_TEST_VIEW_ROOT
#error "GUNGNIR_TEST_VIEW_ROOT is required"
#endif

class User : public gungnir::Model<User> {
public:
    inline static constexpr gungnir::Table table{"users"};
    inline static constexpr auto fillable =
        gungnir::Fillable{"name"};

    gungnir::PrimaryKey<gungnir::Integer> id;
    gungnir::Field<gungnir::String> name;
};

template <>
struct gungnir::model::Generated<User> {
    inline static constexpr auto attributes = std::tuple{
        gungnir::model::attribute("id", &User::id),
        gungnir::model::attribute("name", &User::name)
    };

    inline static constexpr auto relations = std::tuple{};
};

int main() {
    using namespace gungnir;

    orm::Collection<User> users;
    users.push(User::hydrate({
        {"id", Int64{1}},
        {"name", String{"Justin"}}
    }));
    users.push(User::hydrate({
        {"id", Int64{2}},
        {"name", String{"<Admin>"}}
    }));

    view::Data data{
        {"title", "Users"},
        {"users", users},
        {"unsafe", "<script>alert(1)</script>"}
    };

    view::Engine engine;
    const auto inline_html = engine.render_text(
        "<strong>{{ title }}</strong>"
        "{{#each users}}[{{ name }}]{{/each}}",
        data
    );

    assert(
        inline_html ==
        "<strong>Users</strong>[Justin][&lt;Admin&gt;]"
    );

    Application app;
    app.view_root(GUNGNIR_TEST_VIEW_ROOT);

    const auto response = Response::view(
        "users/index",
        data
    );

    assert(response.status() == 200);
    assert(
        response.header("content-type") ==
        "text/html; charset=utf-8"
    );
    assert(
        response.body().find("<h1>Users</h1>") !=
        std::string::npos
    );
    assert(
        response.body().find("<li>Justin</li>") !=
        std::string::npos
    );
    assert(
        response.body().find("<li>&lt;Admin&gt;</li>") !=
        std::string::npos
    );
    assert(
        response.body().find(
            "&lt;script&gt;alert(1)&lt;/script&gt;"
        ) != std::string::npos
    );

    return 0;
}
