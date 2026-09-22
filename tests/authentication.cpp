#include <cassert>
#include <optional>

#include <gungnir/auth/authentication.hpp>

int main() {
    using namespace gungnir;

    auth::Manager manager;
    manager.guard("api", auth::Guard{[](std::string_view credential) -> std::optional<auth::Identity> {
        if (credential != "valid-token") {
            return std::nullopt;
        }
        auth::Identity identity;
        identity.id = "42";
        identity.roles.insert("user");
        return identity;
    }});
    manager.default_guard("api");

    assert(!manager.authenticate("invalid-token"));

    const auto identity = manager.authenticate("valid-token");
    assert(identity);
    assert(identity->id == "42");
    assert(identity->role("user"));

    auth::Context context;
    assert(context.guest());
    context.login(*identity);
    assert(context.check());
    assert(context.user() != nullptr);
    assert(context.user()->id == "42");
    context.logout();
    assert(context.guest());

    return 0;
}
