#include "structured_orm_program.cpp"
#include <gungnir/database/sqlite.hpp>
#include <gungnir/testing/http.hpp>
#include <cassert>
#include <memory>
#include <vector>

int main() {
    using namespace gungnir;
    database::Settings settings;
    settings.database = ":memory:";
    settings.backend = database::Backend::sqlite;
    database::Manager manager;
    manager.add("default", settings.backend,
        [settings] { return std::make_shared<database::SQLiteDriver>(settings); },
        database::PoolOptions{.size = 1});
    database::runtime::use(manager);

    CreateOrmTables schema;
    migration::DatabaseRepository repository;
    migration::Runner migrations{repository};
    assert(migrations.migrate({{"CreateOrmTables", &schema}}) == 1);

    auto ada = createUser("Ada"), freya = createUser("Freya"), empty = createUser("Empty");
    const auto ada_id = ada.id.get(), freya_id = freya.id.get();
    auto post = createPost(ada_id, "Gungnir");
    createPost(ada_id, "C++23");
    createPost(freya_id, "Frameworks");
    createProfile(ada_id, "Ada profile");
    createComment(post.id.get(), "First comment");
    auto admin = createRole("admin"), editor = createRole("editor");
    assert(attachRole(ada_id, admin.id.get()) == 1);
    assert(attachRole(ada_id, editor.id.get()) == 2);

    std::vector<orm::QueryEvent> queries;
    orm::listen([&](const auto& event) { queries.push_back(event); });
    const auto users = loadedUsers();
    assert(users.size() == 3);
    // Parent (1), posts and nested comments (2), profile (1), pivot and roles
    // (2), and two through relationships (4), independent of parent count.
    assert(queries.size() == 10);
    for (const auto& user : users) {
        assert(user.profile.loaded() && user.posts.loaded() && user.roles.loaded());
        assert(user.comments.loaded() && user.firstComment.loaded());
        if (user.id.get() == ada_id) {
            assert(user.posts.size() == 2 && user.roles.size() == 2 && user.comments.size() == 1);
            assert(user.profile.get().display_name.get() == "Ada profile");
            assert(user.firstComment.get().body.get() == "First comment");
            for (const auto& item : user.posts.get()) assert(item.comments.loaded());
        } else if (user.id.get() == empty.id.get()) {
            assert(user.posts.empty() && user.roles.empty() && user.profile.empty());
            assert(user.comments.empty() && user.firstComment.empty());
        }
    }
    assert(queries.size() == 10); // Loaded traversal performs no lazy queries.
    orm::stop_listening();

    auto inverse = Post::with("author").first_or_fail();
    assert(inverse.author.loaded() && inverse.author.get().id.get() == ada_id);
    bool not_loaded = false;
    try { static_cast<void>(requireUser(ada_id).posts.get()); }
    catch (const RelationNotLoaded&) { not_loaded = true; }
    assert(not_loaded);
    assert(postCount(ada_id) == 2 && roleCount(ada_id) == 2);
    assert(storedRelations().first().roles.loaded() && noUsers().empty());
    assert(detachRole(ada_id, editor.id.get()) == 1 && roleCount(ada_id) == 1);
    assert(detachRoles(ada_id) == 1 && roleCount(ada_id) == 0);
    assert(attachNamedRole(ada_id, "admin") == 1 && namedRoleCount(ada_id) == 1);
    assert(loadedNamedRoles(ada_id).front().name.get() == "admin");

    assert(filteredUsers().size() == 3 && selectedUsers().size() == 3);
    assert(listedUsers(ada_id, freya_id).size() == 2);
    assert(findUser(ada_id) && !findUser(9999));
    assert(firstUser() && requireFirstUser().exists() && queryFind(ada_id) == ada_id);
    assert(userPage().data.size() == 1 && pageTotal() == 3);
    assert(collectionFilter().size() == 3 && collectionReject().size() == 1);
    assert(collectionPredicates() && collectionFirst() == "Ada");
    assert(collectionSum() == ada_id + freya_id + empty.id.get() && collectionReduce() == collectionSum());
    assert(collectionFind(ada_id) && !collectionFind(9999));
    assert(collectionSizes() == 2 && collectionChunks() == 2);
    assert(savedName(ada_id, "Ada updated") && requireUser(ada_id).name.get() == "Ada updated");
    assert(savedSettings(ada_id));
    const auto hydrated = requireUser(ada_id);
    assert(!hydrated.dirty() && hydrated.settings.get().get("theme")->string() == "silver");
    const auto serialized = http::make_json(hydrated);
    assert(!serialized.get("password") && serialized.get("settings")->is_object());
    assert(serialized.get("settings")->get("theme")->string() == "silver");
    assert(bulkName(ada_id) == 1 && requireUser(ada_id).name.get() == "updated");
    assert(removeUser(ada_id) && !findUser(ada_id) && deletedUsers() == 1);
    assert(restoreUser(ada_id) == 1 && findUser(ada_id) && deletedUsers() == 0);

    auto tenant = createTenant("tenant-uuid");
    createProject(tenant.uuid.get());
    assert(tenantProjects(tenant.uuid.get()).size() == 1);
    const auto tenants = Tenant::with("projects").get();
    assert(tenants.first().projects.size() == 1);
    const auto projects = Project::with("owner").get();
    assert(projects.first().owner.get().uuid.get() == tenant.uuid.get());
    auto root = createNode(std::nullopt);
    createNode(root.id.get());
    assert(nodeChildren(root.id.get()).size() == 1);
    const auto roots = Node::with({"children", "parent"}).get();
    assert(roots.size() == 2 && roots[0].children.size() == 1 && roots[0].parent.empty());

    routing::Router router;
    UserApi api;
    router.get("/users", [&] { return api.index(); });
    testing::Http client{router};
    const auto response = client.get("/users");
    assert(response.status() == 200 && Json::parse(response.body()).as_array().size() == 3);
    assert(response.body().find("secret") == String::npos);

    bool mass_assignment = false;
    try { static_cast<void>(User::create({{"id", Int64{42}}})); } catch (const MassAssignmentError&) { mass_assignment = true; }
    assert(mass_assignment);
    assert(manager.pool_stats().leased == 0);
    assert(migrations.rollback({{"CreateOrmTables", &schema}}) == 1);
    database::runtime::clear();
}
