#include "structured_orm_program.cpp"
#include <cassert>
#include <tuple>

namespace {

void nullable_model_updates_preserve_values_and_dirty_tracking() {
    using namespace gungnir;
    auto counter = NullableCounter::hydrate({
        {"count", Int64{10}}, {"fraction", Double{1.5}}, {"label", String{"ready"}}
    });
    assert(!counter.dirty());
    const auto check_integer_update = [&](auto update, Int64 returned, Int64 stored) {
        counter.count = Int64{10};
        counter.clean();
        assert((counter.*update)() == returned);
        assert(counter.count.get() == stored && counter.count.original() == 10);
        assert(counter.count.initialized() && counter.is_dirty("count"));
        const auto payload = http::make_json(counter);
        assert(payload.get("count")->string() == std::to_string(stored));
        counter.count.reset();
        assert(counter.count.get() == 10 && !counter.dirty());
    };
    check_integer_update(&NullableCounter::add, 12, 12);
    check_integer_update(&NullableCounter::subtract, 8, 8);
    check_integer_update(&NullableCounter::multiply, 20, 20);
    check_integer_update(&NullableCounter::divide, 5, 5);
    check_integer_update(&NullableCounter::increment, 10, 11);
    check_integer_update(&NullableCounter::decrement, 10, 9);

    assert(counter.addFraction() == 2.0 && counter.fraction.get() == 2.0);
    assert(counter.is_dirty("fraction") && counter.fraction.original() == 1.5);
    counter.clean();
    assert(counter.append() == "ready!" && counter.label.get() == "ready!");
    assert(counter.is_dirty("label") && counter.label.original() == "ready");
    assert(http::make_json(counter).get("label")->string() == "ready!");
    counter.clean();
    assert(counter.appendPlain() == "plain!" && counter.is_dirty("plain_label"));
    assert(appendExternalLabel(counter) == "plain!!" && counter.plain_label.get() == "plain!");

    NullableCounter empty;
    assert(empty.add() == -1 && empty.subtract() == -1);
    assert(empty.multiply() == -1 && empty.divide() == -1);
    assert(empty.increment() == -1 && empty.decrement() == -1);
    assert(empty.addFraction() == -1.0 && empty.append() == "missing");
    assert(!empty.count.initialized() && !empty.dirty());
    empty.setCount(4);
    assert(empty.count.initialized() && empty.count.get() == 4 && empty.is_dirty("count"));
    empty.clean();
    empty.replaceCount();
    assert(empty.count.get() == 20 && empty.is_dirty("count"));
    empty.clean();
    empty.clearCount();
    assert(empty.count.initialized() && !empty.count.get() && empty.is_dirty("count"));
    assert(http::make_json(empty).get("count")->is_null());
    empty.clean();
    assert(empty.increment() == -1 && !empty.dirty());
    empty.setCount(std::nullopt);
    assert(!empty.count.get() && !empty.dirty());
}

} // namespace

int main() {
    using namespace gungnir;
    nullable_model_updates_preserve_values_and_dirty_tracking();
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
