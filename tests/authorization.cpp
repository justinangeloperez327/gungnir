#include <cassert>

#include <gungnir/auth/authentication.hpp>

int main() {
    using namespace gungnir;

    auth::Authorization authorization;
    authorization.define("posts.update", [](const auth::Identity& identity) {
        return identity.role("editor")
            ? auth::Decision::allow()
            : auth::Decision::deny("Editor role required");
    });

    auth::Identity editor;
    editor.id = "1";
    editor.roles.insert("editor");

    auth::Identity viewer;
    viewer.id = "2";
    viewer.roles.insert("viewer");

    assert(authorization.allows("posts.update", editor));
    assert(authorization.denies("posts.update", viewer));
    assert(authorization.denies("missing", editor));

    const auto denied = authorization.inspect("posts.update", viewer);
    assert(!denied.allowed);
    assert(denied.message == "Editor role required");

    auth::Context context;
    context.login(editor);
    auth::authorize(authorization, context, "posts.update");

    context.logout();
    bool rejected = false;
    try {
        auth::authorize(authorization, context, "posts.update");
    } catch (const auth::AuthorizationError&) {
        rejected = true;
    }
    assert(rejected);

    return 0;
}
