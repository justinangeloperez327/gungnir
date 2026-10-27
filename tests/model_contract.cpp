#include "model_contract_program.cpp"
#include <gungnir/database/sqlite.hpp>
#include <cassert>
#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace {
using namespace gungnir;

void assignment_defaults_and_accessors() {
    ContractParent fresh;
    assert(fresh.name.get() == "Guest" && fresh.enabled.get() && fresh.score.get() == 2);
    assert(fresh.settings.get().get("theme")->string() == "blue");
    bool rejected = false;
    try { fresh.fill({{"name",String{"changed"}},{"secret",true}}); }
    catch (const MassAssignmentError&) { rejected = true; }
    assert(rejected && fresh.name.get() == "Guest");
    fresh.fill({{"name",String{"Ada"}}});
    const auto json = http::make_json(fresh);
    assert(json.get("label")->string() == "Ada!");
    assert(!json.get("secret") && !json.get("hiddenLabel"));
    auto visible = ContractVisible::hydrate({{"id",Int64{1}},{"name",String{"Ada"}},{"password",String{"secret"}}});
    const auto limited = http::make_json(visible);
    assert(limited.as_object().size() == 2 && limited.get("name") && limited.get("label"));
    assert(!limited.get("password"));
    ContractCounter counter;
    assert(counter.next() == 0 && counter.count.initialized() && counter.dirty());
    assert(counter.attributes().contains("count"));
    assert(counter.add() == 3 && counter.append() == "!");
    assert(counter.is_dirty("text"));
    static_assert(std::is_const_v<std::remove_reference_t<decltype(counter.count.get())>>);
    ContractDefaultKey keyed;
    assert(keyed.id.get() == 7 && !keyed.id.incrementing());
    assert(!keyed.replicate().id.initialized());
    const auto partial = ContractDefaultKey::hydrate({{"name",String{"partial"}}});
    assert(!partial.primary_key_value());
    ContractAllow allowed;
    allowed.fill({{"count",Int64{4}}});
    assert(allowed.count.get() == 4);
    rejected = false;
    try { allowed.fill({{"text",String{"blocked"}}}); } catch (const MassAssignmentError&) { rejected = true; }
    assert(rejected);
    ContractClosed closed;
    rejected = false;
    try { closed.fill({{"name",String{"blocked"}}}); } catch (const MassAssignmentError&) { rejected = true; }
    assert(rejected);
    const auto filled = fillContractParent(fresh);
    assert(filled.name.get() == "filled" && filled.secret.get());
    ContractCasts casts;
    assert(casts.amount.get().string() == "0.10" && casts.active.get());
    assert(casts.owner.get() == 2 && casts.rate.get() == 1.5 && casts.initial_balance.get() == -2);
    assert(http::make_json(casts).get("settings")->is_array());
    assert(casts.starts_at.get() == "2026-10-26 00:00:00");
    const auto hydrated = ContractCasts::hydrate({{"amount",Double{0.1}},{"active",String{"1"}},{"owner",String{"2"}},{"rate",String{"1.5"}}});
    assert(hydrated.amount.get().string() == "0.1" && hydrated.active.get() && hydrated.owner.get() == 2 && hydrated.rate.get() == 1.5);
    assert(!hydrated.dirty());
    bool bad_cast = false;
    try { static_cast<void>(ContractCasts::hydrate({{"active",String{"not-a-boolean"}}})); }
    catch (const std::invalid_argument&) { bad_cast = true; }
    assert(bad_cast);
}

void schema(database::Connection& connection) {
    for (const auto* sql : {
        "CREATE TABLE contract_parents (id INTEGER PRIMARY KEY, name TEXT, enabled INTEGER, score INTEGER, settings TEXT, secret INTEGER, created_on TEXT, modified_on TEXT, deleted_at TEXT)",
        "CREATE TABLE contract_children (id INTEGER PRIMARY KEY, parent_id INTEGER, name TEXT, created_at TEXT, updated_at TEXT)",
        "CREATE TABLE contract_roles (id INTEGER PRIMARY KEY, name TEXT)",
        "CREATE TABLE contract_parent_role (parent_id INTEGER, role_id INTEGER)",
        "CREATE TABLE contract_notes (id INTEGER PRIMARY KEY, child_id INTEGER, name TEXT)",
        "CREATE TABLE contract_empty (id INTEGER PRIMARY KEY)",
        "CREATE TABLE contract_counters (id INTEGER PRIMARY KEY, count INTEGER, text TEXT)",
        "CREATE TABLE contract_default_keys (id INTEGER PRIMARY KEY, name TEXT)",
        "CREATE TABLE contract_loops (id INTEGER PRIMARY KEY, parent_id INTEGER)",
        "CREATE TABLE contract_touch_loops (id INTEGER PRIMARY KEY, parent_id INTEGER, created_at TEXT, updated_at TEXT)"
    }) connection.execute(sql);
}

void timestamps_loading_counts_and_pagination(database::Connection& connection) {
    auto parent = createContractParent("Ada");
    createContractParent("Freya"); createContractParent("Loki");
    assert(std::regex_match(*parent.created_on.get(),std::regex{R"(\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2}\.\d{6})"}));
    const String sentinel = "2000/01/01 00:00:00.000000";
    connection.execute("UPDATE contract_parents SET modified_on = ? WHERE id = ?",{sentinel,parent.id.get()});
    auto child = createContractChild(parent.id.get(),"one");
    createContractChild(parent.id.get(),"two");
    assert(ContractParent::find(parent.id.get())->modified_on.get() != sentinel);
    connection.execute("UPDATE contract_parents SET modified_on = ? WHERE id = ?",{sentinel,parent.id.get()});
    child.name = "updated";
    assert(child.save());
    assert(ContractParent::find(parent.id.get())->modified_on.get() != sentinel);
    auto roles = createContractRoles();
    assert(roles.size() == 2 && findContractRoles(roles[0].id.get(),roles[1].id.get()).size() == 2);
    connection.execute("INSERT INTO contract_parent_role VALUES (?, ?)",{parent.id.get(),roles[0].id.get()});
    connection.execute("INSERT INTO contract_notes(child_id,name) VALUES (?, ?)",{child.id.get(),String{"note"}});
    std::size_t queries = 0;
    orm::listen([&](const auto&) { ++queries; });
    const auto loaded = ContractParent::order_by("id").get();
    orm::stop_listening();
    // Parent + automatic children + five batched counts, regardless of parent count.
    // Child defaults also request its parent count once, adding one aggregate.
    assert(queries == 8 && loaded.size() == 3);
    const auto& ada = loaded[0];
    assert(ada.children.loaded() && ada.children.size() == 2);
    assert(ada.relation_count("children") == 2 && ada.relation_count("firstChild") == 2);
    assert(ada.relation_count("roles") == 1 && ada.relation_count("notes") == 1 && ada.relation_count("firstNote") == 1);
    assert(ada.children.first().relation_count("parent") == 1);
    assert(loaded[1].relation_count("children") == 0);
    const auto json = http::make_json(ada);
    assert(json.get("children_count")->string() == "2" && json.get("label")->string() == "Ada!");
    assert(countContractChildren(parent.id.get()) == 2);
    const auto page = defaultContractPage();
    assert(page.per_page == 2 && page.total == 3 && page.data.size() == 2);
    assert(ContractParent::query().paginate(1,1).data.size() == 1);
    const auto copied = copyContractParent(ada);
    assert(!copied.exists() && !copied.created_on.initialized() && !copied.modified_on.initialized());
    assert(freshContractParent(ada)->id.get() == ada.id.get());
    assert(firstContractParent("Ada").id.get() == ada.id.get());
    assert(!updateContractParent("Ada").enabled.get());
    const auto explicit_count = ContractChild::query().with_count("parent").first_or_fail();
    assert(explicit_count.relation_count("parent") == 1);
    assert(explicitContractCounts().size() == 2);
    const auto stripped = withoutContractRelations(parent.id.get());
    assert(!stripped.children.loaded());
    const auto count_visible = http::make_json(ContractCountVisible::find(parent.id.get()).value());
    assert(count_visible.as_object().size() == 2 && count_visible.get("children_count"));
    auto refreshed = ContractParent::find(parent.id.get()).value();
    assert(refreshed.refresh() && refreshed.children.loaded() && refreshed.relation_count("children") == 2);
    connection.execute("UPDATE contract_parents SET modified_on = ? WHERE id = ?",{sentinel,parent.id.get()});
    assert(child.remove());
    assert(ContractParent::find(parent.id.get())->modified_on.get() != sentinel);
    assert(parent.remove());
    const auto stored_deleted = connection.execute("SELECT deleted_at FROM contract_parents WHERE id = ?",{parent.id.get()});
    assert(std::get<String>(stored_deleted.rows[0].at("deleted_at")) == *parent.deleted_at.get());
    const auto deleted_modified = connection.execute("SELECT modified_on FROM contract_parents WHERE id = ?",{parent.id.get()});
    assert(std::get<String>(deleted_modified.rows[0].at("modified_on")) == *parent.modified_on.get());
    assert(parent.restore());
    assert(ContractParent::find(parent.id.get()) && !parent.trashed());
    assert(ContractParent::where("id",parent.id.get()).remove() == 1);
    const auto deleted = ContractParent::with_deleted().where_key(parent.id.get()).first().value();
    assert(std::regex_match(*deleted.deleted_at.get(),std::regex{R"(\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2}\.\d{6})"}));
    assert(ContractParent::with_deleted().where_key(parent.id.get()).restore() == 1);
}

void cyclic_eager_loading_and_touching_are_bounded(database::Connection& connection) {
    connection.execute("INSERT INTO contract_loops VALUES (1,1)");
    bool rejected = false;
    try { static_cast<void>(ContractLoop::first()); } catch (const ModelMetadataError&) { rejected = true; }
    assert(rejected);
    assert(ContractEmpty::query().get().empty()); // Depth guard was unwound after the error.
    connection.execute("INSERT INTO contract_touch_loops VALUES (1,1,'old','old')");
    auto loop = ContractTouchLoop::first_or_fail();
    assert(loop.touch());
}

void failed_parent_touch_is_reported_and_guard_recovers(database::Connection& connection) {
    auto parent = createContractParent("touch failure");
    auto child = createContractChild(parent.id.get(),"before");
    connection.execute("CREATE TEMP TRIGGER reject_parent_touch BEFORE UPDATE ON contract_parents BEGIN SELECT RAISE(ABORT, 'parent write failed'); END");
    child.name = "after";
    bool rejected = false;
    try { child.save(); } catch (const database::Error&) { rejected = true; }
    assert(rejected);
    connection.execute("DROP TRIGGER reject_parent_touch");
    const String sentinel = "2000/01/01 00:00:00.000000";
    connection.execute("UPDATE contract_parents SET modified_on = ? WHERE id = ?",{sentinel,parent.id.get()});
    child.name = "recovered";
    assert(child.save());
    assert(ContractParent::find(parent.id.get())->modified_on.get() != sentinel);
}

void persistence_identity_and_default_inserts() {
    auto empty = ContractEmpty::create({});
    assert(empty.exists() && empty.primary_key_value());
    auto first = ContractCounter::create({{"count",Int64{1}},{"text",String{"first"}}});
    auto second = ContractCounter::create({{"count",Int64{2}},{"text",String{"second"}}});
    const auto original_id = first.id.get();
    first.id = second.id.get(); first.text = "wrong row";
    first.clean(); // Synchronizing dirty fields cannot change the persisted identity.
    for (const auto operation : {0,1,2}) {
        bool rejected = false;
        try {
            if (operation == 0) first.save();
            else if (operation == 1) first.force_remove();
            else first.refresh();
        } catch (const ModelMetadataError&) { rejected = true; }
        assert(rejected);
    }
    assert(ContractCounter::find(original_id)->text.get() == "first");
    assert(ContractCounter::find(second.id.get())->text.get() == "second");
    ContractDefaultKey key;
    assert(key.save() && ContractDefaultKey::find(7)->name.get() == "default");
}
}

int main() {
    assignment_defaults_and_accessors();
    database::Settings settings;
    settings.database = ":memory:";
    settings.backend = database::Backend::sqlite;
    database::Manager manager;
    manager.add("default",settings.backend,[&] { return std::make_shared<database::SQLiteDriver>(settings); });
    database::runtime::use(manager);
    auto connection = manager.connection();
    auto scope = database::runtime::activate(connection);
    schema(*connection);
    timestamps_loading_counts_and_pagination(*connection);
    cyclic_eager_loading_and_touching_are_bounded(*connection);
    failed_parent_touch_is_reported_and_guard_recovers(*connection);
    persistence_identity_and_default_inserts();
    database::runtime::clear();
}
