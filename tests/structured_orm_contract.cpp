#include "structured_orm_program.cpp"
#include <cassert>
#include <tuple>

int main() {
    using namespace gungnir;
    static_assert(std::tuple_size_v<decltype(model::Generated<User>::relations)> == 6);
    static_assert(std::tuple_size_v<decltype(model::Generated<Post>::relations)> == 2);
    User user;
    user.id = 7;
    user.name = "Ada";
    user.password = "secret";
    user.settings = Json::object({{"theme", "silver"}});
    assert(user.dirty());
    const auto payload = http::make_json(user);
    assert(!payload.get("password") && payload.get("settings")->is_object());
    assert(payload.get("settings")->get("theme")->string() == "silver");
    const auto copy = Json::parse(payload.dump());
    assert(copy == payload);
    user.posts.set({});
    assert(user.posts.loaded() && user.posts.empty());
    user.posts.unload();
    assert(!user.posts.loaded());
    const auto scoped = language::runtime::relationship_query<&User::posts>(user);
    assert(scoped.plan().predicates.front().column == "user_id");
    for (auto backend : {database::Backend::sqlite, database::Backend::postgresql,
                         database::Backend::mysql, database::Backend::mssql,
                         database::Backend::mongodb}) {
        const auto query = scoped.compile(backend);
        assert(!query.text.empty() && query.bindings.size() == 1);
        assert(std::get<Int64>(query.bindings.front()) == 7);
    }
}
