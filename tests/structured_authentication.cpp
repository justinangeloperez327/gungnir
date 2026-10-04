#include "structured_authentication_program.cpp"
#include <gungnir/language/compiler.hpp>
#include <gungnir/auth/login.hpp>

#include <cassert>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

using namespace gungnir;
namespace {
void reject(std::string_view expression, std::string_view code) {
    using namespace gungnir::language;
    const auto source = "controller Invalid { index(Request request) { return " + std::string{expression} + "; } }";
    const auto emitted = Compiler{}.compile(source, "auth-invalid.gnr");
    CompilerOptions options; options.validate_only = true;
    const auto checked = Compiler{}.compile(source, "auth-invalid.gnr", options);
    assert(!emitted.success() && !checked.success());
    assert(emitted.code.empty() && checked.code.empty());
    assert(emitted.diagnostics.size() == checked.diagnostics.size());
    bool found = false;
    for (std::size_t i = 0; i < emitted.diagnostics.size(); ++i) {
        const auto& left = emitted.diagnostics[i];
        const auto& right = checked.diagnostics[i];
        assert(left.code == right.code && left.message == right.message);
        assert(left.location.file == "auth-invalid.gnr" && left.location.line > 0);
        assert(left.location.line == right.location.line && left.location.column == right.location.column);
        assert(left.span.begin_offset == right.span.begin_offset && left.span.end_offset == right.span.end_offset);
        found |= left.code == code;
    }
    if (!found) for (const auto& item : emitted.diagnostics) std::cerr << item.code << ' ' << item.message << '\n';
    assert(found);
}
#ifdef GUNGNIR_WITH_PASSWORD
Task<void> dispatch(Router& router, Request& request, std::optional<Response>& result) {
    result = co_await router.dispatch(request);
}
Response send(Router& router, std::string path, std::string cookies = {}, std::string input = {}) {
    Request request{input.empty() ? http::Method::get : http::Method::post, std::move(path), std::move(input)};
    request.set_header("Content-Type", "application/x-www-form-urlencoded");
    if (!cookies.empty()) request.set_header("Cookie", std::move(cookies));
    std::optional<Response> response;
    language::runtime::wait(dispatch(router, request, response));
    assert(response);
    return std::move(*response);
}
const http::Cookie& cookie(const Response& response, std::string_view name) {
    for (auto it = response.cookies().rbegin(); it != response.cookies().rend(); ++it)
        if (it->name == name) return *it;
    assert(false); std::terminate();
}
std::string session_cookie(const Response& response) {
    const auto& current = cookie(response, "gungnir_session");
    return current.name + "=" + current.value;
}
bool authenticated(const Response& response) {
    return Json::parse(response.body()).get("authenticated")->dump() == "true";
}
class Tokens final : public auth::RememberStore {
    auth::MemoryRememberStore memory_;
public:
    std::string last_digest;
    void put(std::string digest, std::string identity, std::chrono::system_clock::time_point expires) override {
        last_digest = digest;
        memory_.put(std::move(digest), std::move(identity), expires);
    }
    std::optional<std::string> consume(std::string_view digest) override { return memory_.consume(digest); }
    void revoke(std::string_view digest) override { memory_.revoke(digest); }
};
#endif
}

int main() {
    reject("json(auth.attempt(request))", "GNR2209");
    reject("json(auth.attempt(request, {\"email\":\"x\"}))", "GNR2201");
    reject("json(auth.attempt(request, {\"email\":1,\"password\":\"x\"}))", "GNR2201");
    reject("json(auth.attempt(request, [], false))", "GNR2201");
    reject("json(auth.attempt(request, {\"email\":\"x\",\"password\":\"x\"}, \"1\"))", "GNR2201");
    reject("json(auth.logout())", "GNR2209");
    reject("json(auth)", "GNR2201");
    reject("json({\"service\":auth})", "GNR2201");
    reject("json([auth,1])", "GNR2201");
    reject("text(Password::hash(1))", "GNR2201");
    reject("json(Password::verify(\"x\"))", "GNR2209");
    reject("text(Password.hash(\"x\"))", "GNR2201");
#ifdef GUNGNIR_WITH_PASSWORD
    const auto encoded = hashPassword("correct-password");
    assert(checkPassword("correct-password", encoded));
    assert(!checkPassword("wrong-password", encoded));
    assert(!checkPassword("correct-password", "not-a-hash"));
    assert(!rehashPassword(encoded) && rehashPassword("not-a-hash"));
    bool enabled = true;
    const auto identities = [&](std::string_view id) -> std::optional<auth::Identity> {
        if (!enabled || id != "7") return std::nullopt;
        return auth::Identity{.id="7", .roles={"editor"}, .attributes={{"name","Freya"}}};
    };
    auto tokens = std::make_shared<Tokens>();
    auto guard = std::make_shared<auth::SessionGuard>(
        [&](std::string_view email) -> std::optional<auth::PasswordIdentity> {
            if (email != "freya@example.test") return std::nullopt;
            return auth::PasswordIdentity{*identities("7"), encoded};
        }, identities, tokens);
    Application app;
    app.provider<ServicesProvider>(ServiceOptions{.authentication=guard});
    app.boot();
    auto store = std::make_shared<session::MemoryStore>();
    app.router().use(session::middleware(store));
    app.router().use(auth::session(identities));
    app.router().use(auth::guard(guard));
    AuthenticationTrace trace;
    app.router().use([&](Request& request, Next next) -> Task<Response> {
        co_return co_await trace.handle(request, std::move(next));
    });
    LoginController controller;
    std::optional<Request> retained;
    app.router().post("/login", [&](Request& request) { return controller.store(request); });
    app.router().get("/logout", [&](Request& request) { return controller.logout(request); });
    app.router().get("/status", [&](Request& request) { return controller.status(request); });
    app.router().get("/literal", [&](Request& request) { return controller.literalCredentials(request); });
    app.router().get("/capture", [&](Request& request) { retained = request; return controller.status(request); });
    app.router().post("/failed-capture", [&](Request& request) { retained = request; return controller.store(request); });
    app.router().get("/malformed", [&](Request& request) {
        return controller.dynamicCredentials(request, Json::object({{"email", "secret-input"}, {"password", Int64{7}}}));
    });
    std::weak_ptr<auth::SessionGuard> owner = guard;
    guard.reset();
    assert(!owner.expired());
    Application misconfigured;
    misconfigured.provider<ServicesProvider>(ServiceOptions{.authentication=owner.lock()});
    misconfigured.boot();
    misconfigured.router().use(session::middleware(std::make_shared<session::MemoryStore>()));
    misconfigured.router().use(auth::session(identities));
    misconfigured.router().get("/attempt", [&](Request& request) {
        bool rejected = false;
        try { (void)controller.literalCredentials(request); }
        catch (const std::logic_error& error) { rejected = std::string{error.what()}.find("guard middleware") != std::string::npos; }
        assert(rejected && !request.authenticated());
        return Response::text("configuration rejected");
    });
    assert(send(misconfigured.router(), "/attempt").body() == "configuration rejected");
    auto different_guard = std::make_shared<auth::SessionGuard>(
        [](std::string_view) -> std::optional<auth::PasswordIdentity> { return std::nullopt; }, identities, tokens);
    Application mismatched;
    mismatched.provider<ServicesProvider>(ServiceOptions{.authentication=different_guard});
    mismatched.boot();
    mismatched.router().use(session::middleware(std::make_shared<session::MemoryStore>()));
    mismatched.router().use(auth::session(identities));
    mismatched.router().use(auth::guard(owner.lock()));
    mismatched.router().get("/attempt", [&](Request& request) {
        bool rejected = false;
        try { (void)controller.literalCredentials(request); } catch (const std::logic_error&) { rejected = true; }
        assert(rejected && !request.authenticated());
        return Response::text("mismatched guard rejected");
    });
    assert(send(mismatched.router(), "/attempt").body() == "mismatched guard rejected");
    const auto anonymous = send(app.router(), "/status");
    assert(!authenticated(anonymous));
    const auto original = session_cookie(anonymous);
    const auto validation = send(app.router(), "/login", original, "email=bad&password=x");
    assert(validation.status() == 422);
    const auto malformed = send(app.router(), "/malformed", original);
    assert(malformed.status() == 400 && malformed.body().find("secret-input") == std::string::npos);
    const auto unknown = send(app.router(), "/login", original, "email=unknown%40example.test&password=wrong");
    assert(unknown.header("Location") == "/login");
    const auto failed = send(app.router(), "/login", original, "email=freya%40example.test&password=wrong");
    assert(failed.header("Location") == "/login" && !authenticated(send(app.router(), "/status", original)));
    const auto login = send(app.router(), "/login", original, "email=freya%40example.test&password=correct-password&remember=1");
    assert(login.header("Location") == "/welcome");
    const auto signed_session = session_cookie(login);
    assert(signed_session != original && !authenticated(send(app.router(), "/status", original)));
    assert(authenticated(send(app.router(), "/status", signed_session)));
    assert(authenticated(send(app.router(), "/capture", signed_session)));
    assert(retained && retained->authenticated() && !retained->auth().active_guard());
    bool finished = false;
    try { (void)controller.logout(*retained); } catch (const std::logic_error&) { finished = true; }
    assert(finished && retained->authenticated());
    assert(send(app.router(), "/failed-capture", {}, "email=bad&password=x").status() == 422);
    assert(retained && !retained->auth().active_guard());
    const auto remembered = cookie(login, "gungnir_remember");
    assert(remembered.value.size() == 64 && remembered.secure && remembered.http_only);
    assert(remembered.max_age->count() > 0 && tokens->last_digest != remembered.value);
    assert(tokens->last_digest == auth::Password::token_digest(remembered.value));
    const auto recalled = send(app.router(), "/status", "gungnir_remember=" + remembered.value);
    assert(authenticated(recalled));
    const auto replacement = cookie(recalled, "gungnir_remember");
    assert(replacement.value != remembered.value && replacement.value.size() == 64);
    const auto replay = send(app.router(), "/status", "gungnir_remember=" + remembered.value);
    assert(!authenticated(replay) && cookie(replay, "gungnir_remember").max_age->count() == 0);
    const auto recalled_session = session_cookie(recalled);
    const auto logout = send(app.router(), "/logout", recalled_session + "; gungnir_remember=" + replacement.value);
    assert(logout.header("Location") == "/login");
    assert(session_cookie(logout) != recalled_session && cookie(logout, "gungnir_remember").max_age->count() == 0);
    assert(!authenticated(send(app.router(), "/status", recalled_session)));
    assert(!authenticated(send(app.router(), "/status", "gungnir_remember=" + replacement.value)));
    const auto ordinary = send(app.router(), "/login", {}, "email=freya%40example.test&password=correct-password");
    assert(cookie(ordinary, "gungnir_remember").max_age->count() == 0);
    const auto remembered_again = send(app.router(), "/login", {}, "email=freya%40example.test&password=correct-password&remember=1");
    const auto revoked = cookie(remembered_again, "gungnir_remember").value;
    enabled = false;
    const auto disabled = send(app.router(), "/status", "gungnir_remember=" + revoked);
    assert(!authenticated(disabled) && cookie(disabled, "gungnir_remember").max_age->count() == 0);
    enabled = true;
    assert(!authenticated(send(app.router(), "/status", "gungnir_remember=" + revoked)));
    Request unconfigured{http::Method::post,"/login"};
    bool missing = false;
    try { (void)controller.literalCredentials(unconfigured); } catch (const std::logic_error&) { missing = true; }
    assert(missing);
    bool null_guard = false;
    try { (void)auth::guard({}); } catch (const std::invalid_argument&) { null_guard = true; }
    assert(null_guard);
#else
    bool disabled = false;
    try { (void)hashPassword("x"); } catch (const std::logic_error& error) {
        disabled = std::string{error.what()}.find("GUNGNIR_WITH_PASSWORD") != std::string::npos;
    }
    assert(disabled);
    auto unavailable = std::make_shared<auth::SessionGuard>(
        [](std::string_view) -> std::optional<auth::PasswordIdentity> { return std::nullopt; },
        [](std::string_view) -> std::optional<auth::Identity> { return std::nullopt; });
    bool middleware_disabled = false;
    try { (void)auth::guard(unavailable); } catch (const std::logic_error& error) {
        middleware_disabled = std::string{error.what()}.find("GUNGNIR_WITH_PASSWORD") != std::string::npos;
    }
    assert(middleware_disabled);
#endif
}
