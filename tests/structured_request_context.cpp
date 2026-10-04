#include "structured_context_program.cpp"
#include <gungnir/language/compiler.hpp>
#include <gungnir/orm/orm.hpp>
#include <gungnir/auth/login.hpp>

#include <cassert>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

using namespace gungnir;
namespace {
Task<void> dispatch(Router& router, Request& request, std::optional<Response>& result) {
    result = co_await router.dispatch(request);
}
Response send(Router& router, std::string path, std::string cookies = {}) {
    Request request{http::Method::get, std::move(path)};
    if (!cookies.empty()) request.set_header("Cookie", std::move(cookies));
    std::optional<Response> result;
    language::runtime::wait(dispatch(router, request, result));
    assert(result);
    return std::move(*result);
}
std::string session_cookie(const Response& response) {
    for (const auto& cookie : response.cookies())
        if (cookie.name == "gungnir_session") return cookie.name + "=" + cookie.value;
    assert(false);
    return {};
}
Account account(Int64 id) {
    return Account::hydrate({{"id", id}, {"name", String{"Freya"}}, {"secret", String{"private-hash"}}});
}
void reject(std::string_view source, std::string_view code) {
    using namespace gungnir::language;
    const auto emitted = Compiler{}.compile(source, "context-invalid.gnr");
    CompilerOptions options;
    options.validate_only = true;
    const auto checked = Compiler{}.compile(source, "context-invalid.gnr", options);
    assert(!emitted.success() && !checked.success());
    assert(emitted.code.empty() && checked.code.empty());
    assert(emitted.diagnostics.size() == checked.diagnostics.size());
    bool found = false;
    for (std::size_t i = 0; i < emitted.diagnostics.size(); ++i) {
        const auto& left = emitted.diagnostics[i];
        const auto& right = checked.diagnostics[i];
        assert(left.code == right.code && left.message == right.message);
        assert(left.location.file == "context-invalid.gnr");
        assert(left.location.line == right.location.line && left.location.column == right.location.column);
        assert(left.span.begin_offset == right.span.begin_offset && left.span.end_offset == right.span.end_offset);
        assert(left.location.line > 0 && left.location.column > 0);
        found |= left.code == code;
    }
    if (!found) for (const auto& diagnostic : emitted.diagnostics)
        std::cerr << diagnostic.code << ' ' << diagnostic.message << '\n';
    assert(found);
}
void reject_expression(std::string_view expression, std::string_view code) {
    reject("controller Invalid { index(Request request) { return " + std::string{expression} + "; } }", code);
}
}

int main() {
    ContextAccount controller;
    Request unconfigured{http::Method::get, "/"};
    assert(!identitySnapshot(unconfigured));
    assert(controller.snapshot(unconfigured).body() == "null");
    assert(controller.user(unconfigured).status() == 401);
    bool missing_context = false;
    try { (void)captureSession(unconfigured); }
    catch (const std::logic_error&) { missing_context = true; }
    assert(missing_context);
    bool null_handle = false;
    try { writeCaptured({}, "en"); }
    catch (const std::logic_error&) { null_handle = true; }
    assert(null_handle);

    Application app;
    app.provider<ServicesProvider>();
    app.on_boot([](Application& current) {
        register_policy<AccountPolicy>(current, std::make_shared<AccountPolicy>());
    });
    app.boot();
    const auto identity = [](std::string_view id) -> std::optional<auth::Identity> {
        if (id != "7") return std::nullopt;
        const auto model = account(7);
        return auth::Identity{.id=std::to_string(model.id.get()), .roles={"editor","admin"},
            .attributes={{"name", model.name.get()}}};
    };
    auto authorization = app.container().resolve<auth::ResourceAuthorization>();
    authorization->actor<Account>([](const auth::Identity& current) -> std::optional<Account> {
        return current.id == "7" ? std::optional{account(7)} : std::nullopt;
    });
    auto store = std::make_shared<session::MemoryStore>();
    app.router().use(session::middleware(store));
    app.router().use(auth::session(identity));
    app.router().get("/write", [&](Request& request) { return controller.write(request); });
    app.router().get("/read", [&](Request& request) { return controller.read(request); });
    app.router().get("/forget", [&](Request& request) { return controller.forget(request); });
    app.router().get("/clear", [&](Request& request) { return controller.clear(request); });
    app.router().get("/rotate", [&](Request& request) { return controller.rotate(request); });
    app.router().get("/invalidate", [&](Request& request) { return controller.invalidate(request); });
    app.router().get("/user", [&](Request& request) { return controller.user(request); });
    app.router().get("/snapshot", [&](Request& request) { return controller.snapshot(request); });
    app.router().get("/nested-snapshot", [&](Request& request) { return controller.nestedSnapshot(request); });
    std::shared_ptr<session::Session> retained;
    std::optional<auth::Identity> copied;
    app.router().get("/capture", [&](Request& request) {
        retained = captureSession(request);
        copied = identitySnapshot(request);
        return Response::text("captured");
    });
#ifdef GUNGNIR_WITH_PASSWORD
    const auto password_hash = auth::Password::hash("correct-password");
    auth::SessionGuard guard{
        [=](std::string_view login) -> std::optional<auth::PasswordIdentity> {
            if (login != "freya@example.test") return std::nullopt;
            return auth::PasswordIdentity{*identity("7"), password_hash};
        }, identity};
    app.router().get("/login", [&](Request& request) {
        auto result = Response::text("logged in");
        assert(guard.attempt(request, result, "freya@example.test", "correct-password"));
        return result;
    });
    app.router().get("/failed-login", [&](Request& request) {
        auto result = Response::text("rejected", 401);
        assert(!guard.attempt(request, result, "freya@example.test", "wrong-password"));
        assert(!request.authenticated());
        return result;
    });
    app.router().get("/logout", [&](Request& request) {
        auto result = Response::text("logged out");
        guard.logout(request, result);
        return result;
    });
#else
    app.router().get("/login", [&](Request& request) {
        request.auth().login(*identity("7"));
        return Response::text("logged in");
    });
    app.router().get("/logout", [](Request& request) {
        request.auth().logout();
        return Response::text("logged out");
    });
#endif
    RequireContextUser require_user;
    auto protected_group = app.router().group("/protected");
    protected_group.middleware([&](Request& request, Next next) -> Task<Response> {
        co_return co_await require_user.handle(request, std::move(next));
    });
    protected_group.get("/own", [&](Request& request) { return controller.protectedAccount(request, account(7)); });
    protected_group.get("/other", [&](Request& request) { return controller.protectedAccount(request, account(8)); });

    const auto written = send(app.router(), "/write");
    assert(written.body() == "saved");
    assert(written.cookies()[0].name == "theme" && written.cookies()[0].http_only);
    assert(http::serialize_cookie(written.cookies()[0]).find("SameSite=Strict") != String::npos);
    auto cookie = session_cookie(written);
    auto data = Json::parse(send(app.router(), "/read", cookie).body());
    assert(data.get("locale")->string() == "en" && data.get("present")->dump() == "true");
    assert(data.get("notice")->string() == "Saved");
    assert(data.get("values")->get("locale")->string() == "en");
    assert(Json::parse(send(app.router(), "/read", cookie).body()).get("notice")->string().empty());
    const auto rotated = send(app.router(), "/rotate", cookie);
    auto rotated_cookie = session_cookie(rotated);
    assert(rotated_cookie != cookie);
    assert(Json::parse(send(app.router(), "/read", rotated_cookie).body()).get("locale")->string() == "en");
    assert(Json::parse(send(app.router(), "/read", cookie).body()).get("locale")->string().empty());
    assert(send(app.router(), "/forget", rotated_cookie).body() == "forgotten");
    assert(Json::parse(send(app.router(), "/read", rotated_cookie).body()).get("present")->dump() == "false");
    assert(send(app.router(), "/write", rotated_cookie).status() == 200);
    assert(send(app.router(), "/clear", rotated_cookie).body() == "cleared");
    assert(Json::parse(send(app.router(), "/read", rotated_cookie).body()).get("values")->as_object().empty());
    assert(send(app.router(), "/protected/own").header("Location") == "/login");
#ifdef GUNGNIR_WITH_PASSWORD
    assert(send(app.router(), "/failed-login").status() == 401);
#endif
    const auto login = send(app.router(), "/login", rotated_cookie);
    auto signed_cookie = session_cookie(login);
    assert(signed_cookie != rotated_cookie);
    data = Json::parse(send(app.router(), "/user", signed_cookie).body());
    assert(data.get("id")->string() == "7" && data.get("editor")->dump() == "true");
    const auto snapshot = send(app.router(), "/snapshot", signed_cookie);
    assert(snapshot.body().find("private-hash") == String::npos);
    data = Json::parse(snapshot.body());
    assert(data.get("roles")->as_array()[0].string() == "admin");
    assert(Json::parse(send(app.router(), "/nested-snapshot", signed_cookie).body()).get("user")->get("id")->string() == "7");
    const auto allowed = send(app.router(), "/protected/own", signed_cookie);
    assert(allowed.status() == 200 && !Json::parse(allowed.body()).get("secret"));
    assert(send(app.router(), "/protected/other", signed_cookie).status() == 403);
    assert(send(app.router(), "/capture", signed_cookie).status() == 200);
    assert(retained && copied && copied->id == "7");
    writeCaptured(retained, "fr");
    assert(retained->get("locale") == "fr");
    const auto logout = send(app.router(), "/logout", signed_cookie);
    assert(session_cookie(logout) != signed_cookie);
    assert(send(app.router(), "/user", signed_cookie).status() == 401);
    assert(copied->id == "7" && copied->role("editor"));
    const auto invalidated = send(app.router(), "/invalidate", session_cookie(logout));
    assert(session_cookie(invalidated) != session_cookie(logout));
    assert(controller.expire().cookies()[0].max_age->count() == 0);
    assert(controller.expireDomain().cookies()[0].domain == "example.test");
    assert(controller.expireDomain().cookies()[0].max_age->count() == 0);
    assert(controller.expireHost().cookies()[0].secure);
    assert(controller.secureCookie().cookies()[0].secure);
    assert(controller.secureCookie().cookies()[0].max_age->count() == 300);
    assert(controller.sameSiteCookie().cookies()[0].same_site == http::SameSite::none);
    for (const auto& options : {R"({"secure":"yes"})", R"({"sameSite":"Unknown"})", R"({"path":"/;unsafe"})", R"({"domain":"bad\r\nInjected: true"})", R"({"sameSite":"None","secure":false})"}) {
        bool rejected = false;
        try { (void)controller.dynamicCookie(Json::parse(options)); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }

    reject_expression("text(request.session(1).id())", "GNR2209");
    reject_expression("text(request.session().get(1))", "GNR2201");
    reject_expression("text(request.session().regenerate(\"predictable\"))", "GNR2209");
    reject_expression("json(request.session())", "GNR2201");
    reject_expression("json({\"session\":request.session()})", "GNR2201");
    reject_expression("json(request.user(1))", "GNR2209");
    reject_expression("text(request.user().id)", "GNR2201");
    reject_expression("text(\"ok\").cookie(1, \"value\")", "GNR2201");
    reject_expression("text(\"ok\").cookie(\"name\")", "GNR2209");
    reject_expression("text(\"ok\").cookie(\"name\", \"value\", {\"secure\":\"yes\"})", "GNR2201");
    reject_expression("text(\"ok\").cookie(\"name\", \"value\", {\"unknown\":true})", "GNR2201");
    reject("controller Invalid { index() { const result = text(\"ok\"); return result.cookie(\"name\", \"value\"); } }", "GNR2208");
    reject("function void invalid(Request request) { const user = request.user(); if (user != null) { user.id = \"8\"; } }", "GNR2208");
    reject("job Invalid { Session context; handle() {} }", "GNR2201");
    reject("job Invalid { List<AuthIdentity> users; handle() {} }", "GNR2201");
}
