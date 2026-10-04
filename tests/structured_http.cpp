#include "structured_http_program.cpp"
#include <gungnir/language/compiler.hpp>
#include <gungnir/testing/http.hpp>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {
void rejects_source(std::string_view source, std::string_view code) {
    using namespace gungnir::language;
    const auto emitted = Compiler{}.compile(source, "http-invalid.gnr");
    CompilerOptions options;
    options.validate_only = true;
    const auto checked = Compiler{}.compile(source, "http-invalid.gnr", options);
    assert(!emitted.success() && !checked.success());
    assert(emitted.code.empty() && checked.code.empty());
    assert(emitted.diagnostics.size() == checked.diagnostics.size());
    bool found = false;
    for (std::size_t i = 0; i < emitted.diagnostics.size(); ++i) {
        const auto& actual = emitted.diagnostics[i];
        const auto& expected = checked.diagnostics[i];
        assert(actual.code == expected.code);
        assert(actual.location.file == "http-invalid.gnr");
        assert(actual.location.line == expected.location.line);
        assert(actual.location.column == expected.location.column);
        assert(actual.location.line > 0 && actual.location.column > 0);
        found |= actual.code == code;
    }
    if (!found) for (const auto& diagnostic : emitted.diagnostics)
        std::cerr << diagnostic.code << ' ' << diagnostic.message << '\n';
    assert(found);
}
void rejects(std::string_view expression, std::string_view code) {
    const auto source = "controller Invalid { index(Request request) { return "
        + std::string{expression} + "; } }";
    rejects_source(source, code);
}
}

int main() {
    using namespace gungnir;
    const auto payload = Json::parse(R"({"name":"Freya","enabled":true,"count":2,"items":[1,false],"missing":null})");
    assert(optionalName(payload) == "Freya");
    assert(optionalName(Json::object({})) == "missing");
    assert(payload.find("missing") && payload.find("missing")->is_null());
    assert(!payload.find("absent"));
    assert(jsonKinds(payload));
    assert(Json::parse(jsonDump(payload)).get("enabled")->is_boolean());
    assert(responseBody() == "second");
    assert(responseStatus() == 202);
    assert(responseHeader() == "kept");
    assert(responseHeaders().at("x-result") == "kept");
    assert(nestedResponse().header("X-Nested") == "yes");
    assert(nestedResponse().header("X-Copy") == "yes");
    assert(selectedResponse(true).body() == "yes");
    assert(selectedResponse(false).header("X-Selected") == "yes");

    Request request{http::Method::post, "/items/7?search=term&name=query", payload.dump()};
    request.set_header("Content-Type", "application/json; charset=utf-8");
    request.set_header("Accept", "application/json");
    request.set_header("User-Agent", "Gungnir test");
    request.set_header("Host", "example.test");
    request.set_header("Cookie", "theme=silver");
    request.set_header("Authorization", "Bearer test-token");
    request.client_ip("192.0.2.1");
    assert(selectedInput(request, {"name"}).at("name") == "Freya");
    assert(objectValues(request).at("enabled").is_boolean());

    HttpApi controller;
    auto echo = controller.echo(request);
    assert(echo.status() == 201 && echo.header("Cache-Control") == "no-store");
    assert(echo.header("X-Generated") == "yes");
    assert(Json::parse(echo.body()).get("items")->as_array()[1].is_boolean());
    auto metadata = Json::parse(controller.metadata(request).body());
    assert(metadata.get("method")->string() == "POST");
    assert(metadata.get("input")->string() == "Freya");
    assert(metadata.get("search")->string() == "term");
    assert(metadata.get("cookie")->string() == "silver");
    assert(metadata.get("bearer")->string() == "test-token");
    assert(metadata.get("clientIp")->string() == "192.0.2.1");
    assert(metadata.get("authenticated")->dump() == "false");
    assert(metadata.get("guest")->dump() == "true");
    assert(metadata.get("isJson")->dump() == "true");
    assert(metadata.get("expectsJson")->dump() == "true");
    assert(metadata.get("only")->as_object().size() == 1);
    assert(!metadata.get("except")->get("name"));

    Request array{http::Method::post, "/", "[1,true,null]"};
    array.set_header("Content-Type", "application/json");
    assert(arraySize(array) == 3);
    Request form{http::Method::post, "/", "name=Freya&count=2"};
    form.set_header("Content-Type", "application/x-www-form-urlencoded");
    assert(Json::parse(controller.formInput(form).body()).get("name")->string() == "Freya");
    assert(controller.empty().status() == 204 && controller.empty().body().empty());
    assert(controller.file().body() == "report");
    assert(controller.file().header("Content-Disposition").find("report.txt") != std::string_view::npos);

    Router router;
    AddResponseHeader add_header;
    router.use([&](Request& current, Next next) -> Task<Response> {
        co_return co_await add_header.handle(current, std::move(next));
    });
    router.get("/items/{id}", [&](Request& current) { return controller.metadata(current); });
    router.post("/echo", [&](Request& current) {
        current.set_header("Content-Type", "application/json");
        return controller.echo(current);
    });
    router.post("/validate", [&](Request& current) {
        current.set_header("Content-Type", "application/json");
        return controller.validateInput(current);
    });
    testing::Http client{router};
    const auto routed = client.get("/items/42");
    assert(routed.header("X-Middleware") == "generated");
    assert(Json::parse(routed.body()).get("id")->string() == "42");
    assert(client.post("/echo", payload.dump()).status() == 201);
    const auto malformed = client.post("/echo", "{private-input");
    assert(malformed.status() == 400);
    assert(malformed.body().find("private-input") == std::string_view::npos);
    assert(client.post("/echo", "").status() == 400);
    assert(client.post("/validate", "{}").status() == 422);

    Router protected_router;
    EnsureActiveUser ensure_active;
    protected_router.use(session::middleware(std::make_shared<session::MemoryStore>()));
    protected_router.use(auth::session([](std::string_view id) -> std::optional<auth::Identity> {
        return auth::Identity{.id=std::string{id}};
    }));
    protected_router.use([](Request& current, Next next) -> Task<Response> {
        if (current.path() == "/signed") {
            current.auth().login(auth::Identity{.id="7"});
            current.secure(true);
        }
        co_return co_await next(current);
    });
    protected_router.use([&](Request& current, Next next) -> Task<Response> {
        co_return co_await ensure_active.handle(current, std::move(next));
    });
    protected_router.get("/private", [] { return Response::text("allowed"); });
    protected_router.get("/signed", [&](Request& current) { return controller.metadata(current); });
    testing::Http guest{protected_router};
    assert(guest.get("/private").header("Location") == "/login");
    metadata = Json::parse(guest.get("/signed").body());
    assert(metadata.get("authenticated")->dump() == "true");
    assert(metadata.get("guest")->dump() == "false");
    assert(metadata.get("hasAuth")->dump() == "true");
    assert(metadata.get("hasSession")->dump() == "true");
    assert(metadata.get("secure")->dump() == "true");
    bool invalid_header = false;
    try { echo.header("X-Test", "unsafe\r\nInjected: true"); }
    catch (const std::invalid_argument&) { invalid_header = true; }
    assert(invalid_header);

    rejects("json(request.json(1))", "GNR2209");
    rejects("json(request.only([1]))", "GNR2201");
    rejects("json(request.only(\"name\"))", "GNR2201");
    rejects("json(request.has(1))", "GNR2201");
    rejects("json(request.authenticated(1))", "GNR2209");
    rejects("text(\"ok\").header(1, \"value\")", "GNR2201");
    rejects("text(\"ok\").header(\"X-Test\", true)", "GNR2201");
    rejects("text(\"ok\").status(\"bad\")", "GNR2201");
    rejects("json(request.json().get(1))", "GNR2201");
    rejects("json(request.json().asArray(1))", "GNR2209");
    rejects("noContent(1)", "GNR2209");
    rejects("download(\"body\")", "GNR2209");
    rejects_source("controller Invalid { index() { const result = text(\"ok\"); return result.header(\"X-Test\", \"value\"); } }", "GNR2208");
}
