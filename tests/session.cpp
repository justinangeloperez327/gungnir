#include <cassert>

#include <gungnir/session/sessions.hpp>

int main() {
    using namespace gungnir;

    session::Session value{"old"};
    value.put("user_id", "42");
    assert(value.has("user_id"));
    assert(value.get("user_id") == "42");

    value.flash("status", "saved");
    assert(value.flashed("status").empty());
    value.age_flash();
    assert(value.flashed("status") == "saved");
    value.age_flash();
    assert(value.flashed("status").empty());

    value.regenerate("new");
    assert(value.id() == "new");
    assert(value.regenerated());
    assert(value.get("user_id") == "42");

    session::MemoryStore store;
    store.save(value);
    const auto loaded = store.load("new");
    assert(loaded);
    assert(loaded->get("user_id") == "42");
    store.erase("new");
    assert(!store.load("new"));

    value.invalidate("rotated");
    assert(value.id() == "rotated");
    assert(!value.has("user_id"));

    return 0;
}
