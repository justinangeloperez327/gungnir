#include "structured_routing_program.cpp"
#include <gungnir/language/compiler.hpp>
#include <gungnir/language/cpp_ir.hpp>
#ifdef GUNGNIR_WITH_SQLITE
#include <gungnir/database/sqlite.hpp>
#endif
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>

using namespace gungnir;
#define CHECK(...) do { if (!(__VA_ARGS__)) { std::cerr << __FILE__ << ":" << __LINE__ << ": failed check: " << #__VA_ARGS__ << std::endl; std::exit(EXIT_FAILURE); } } while (false)
namespace {
void stage(std::string_view name) { std::cerr << "routing: " << name << std::endl; }
void reject(std::string_view routes, std::string_view declarations = "controller Home { index() { return text(\"home\"); } show(int id) { return json(id); } }", bool route_diagnostic = true) {
    const String source = String{declarations} + "\n" + String{routes};
    language::CompilerOptions options; options.validate_only = true;
    const auto emitted = language::Compiler{}.compile(source,"routing-invalid.gnr");
    const auto checked = language::Compiler{}.compile(source,"routing-invalid.gnr",options);
    CHECK(!emitted.success() && !checked.success() && emitted.code.empty() && checked.code.empty());
    CHECK(emitted.diagnostics.size() == checked.diagnostics.size());
    bool found = false;
    for (std::size_t i = 0; i < emitted.diagnostics.size(); ++i) {
        const auto& a = emitted.diagnostics[i]; const auto& b = checked.diagnostics[i];
        CHECK(a.code == b.code && a.message == b.message && a.location.line == b.location.line && a.location.column == b.location.column);
        CHECK(a.span.valid && a.span.begin_offset == b.span.begin_offset && a.span.end_offset == b.span.end_offset);
        found |= a.code == "GNR2320";
    }
    CHECK(found || !route_diagnostic);
}
Task<void> dispatch(Router& router, Request& request, std::optional<Response>& result) { result = co_await router.dispatch(request); }
Response send(Router& router, String path, http::Method method = http::Method::get) {
    Request request{method,std::move(path)};
    std::optional<Response> result;
    language::runtime::wait(dispatch(router,request,result));
    CHECK(result); return std::move(*result);
}
template<class T> void bind(Application& app) { app.bind<T>([](Container& container){return T::make(container);}); }
}

void exercise_routing() {
    stage("validation");
    reject("Route::get(\"/\", Missing::index);");
    reject("Route::get(\"/\", Home::missing);");
    reject("Route::get(\"/\", Home::index);", "controller Home { private index() { return text(\"private\"); } }");
    reject("Route::get(\"/\", Home::index);", "controller Home { int index() { return 1; } }");
    reject("Route::get(\"/\", Home::show);");
    reject("Route::get(\"/{id}/{id}\", Home::show);");
    reject("Route::get(\"/prefix{id}\", Home::show);");
    reject("Route::get(\"relative\", Home::index);");
    reject("Route::get(\"/?secret=value\", Home::index);");
    reject("Route::get(1, Home::index);");
    reject("Route::get(\"/\");");
    reject("Route::get(\"/\", () => text(\"bad\"));");
    reject("Route::get(\"/\", Home::index).unknown(\"option\");");
    reject("Route::get(\"/\", Home::index).whereNumber(\"missing\");");
    reject("Route::get(\"/{id}\", Home::show).where(\"id\", \"[\");");
    reject("Route::get(\"/\", Home::index).name(\"same\"); Route::get(\"/other\", Home::index).name(\"same\");");
    reject("Route::get(\"/\", Home::index).middleware(Home);");
    reject("Route::get(\"/\", Home::index).middleware(\"\");");
    const String middleware_source = "controller Home { index() { return noContent(); } } middleware Pass { async Response handle(Request request, Next next) { return await next(request); } }";
    reject("Route::get(\"/\", Home::index).middleware<Pass?>();",middleware_source);
    reject("Route::get(\"/\", Home::index).middleware<Pass<int>>();",middleware_source);
    reject("Route::fallback(Home::index); Route::fallback(Home::index);");
    reject("Route::prefix(\"/api\").group(() => { Route::fallback(Home::index); });");
    reject("Route::get(\"/\", Home::index).group(() => {});");
    reject("Route::prefix(\"/api\").group((int value) => {});", "controller Home {}", false);
    reject("Route::get(\"/{id}\", Home::show);", "controller Home { show(int? id) { return json(id); } }");
    reject("Route::get(\"/{id}\", Home::show);", "controller Home { show(Cache id) { return noContent(); } }");
    reject("Route::get(\"/\", Home::show);", "controller Home { show(Request first, Request second) { return noContent(); } }");
    reject("Route::get(\"/{id}\", Home::show);", "controller Home { show(int id = 1) { return json(id); } }");
    reject("", "controller Home { index() { Route::get(\"/\", Home::index); return noContent(); } }");
    reject("", "controller Home { index() { return text(Route::url(\"home\", { id: [1] })); } }");
    for (const auto value : {"1","[1]","\"wrong\"","null"}) reject("", String{"controller Home { index() { return text(Route::url(\"home\", "} + value + ")); } }");
    reject("", "function string url(Map<string,List<int>> values) { return Route::url(\"home\", values); }");
    reject("", "controller Home { index() { let route = Route; return noContent(); } }");
    reject("", "model Holder { Route route; }");

    stage("modules and IR");
    language::CompilerOptions options; options.emit_line_directives = false;
    const std::vector<language::SourceFile> modules{
        {"a.gnr","app.home","controller Home { index() { return text(\"home\"); } }"},
        {"b.gnr","app.admin","controller Home { index() { return text(\"admin\"); } }"},
        {"routes.gnr","routes.web","Route::get(\"/\", Home::index);"}};
    CHECK(!language::Compiler{}.compile_sources(modules,options).success());
    auto imported = modules;
    imported.back().source = "import app.admin as Admin; Route::get(\"/\", Admin::Home::index).name(\"admin\");";
    auto selected = language::Compiler{}.compile_sources(imported,options);
    CHECK(selected.success() && selected.validated->routes().size() == 1);
    CHECK(selected.code.find("::gnr::app::admin::Home") != String::npos);
    const std::vector<language::SourceFile> model_modules{
        {"parent.gnr","app.parent","model Parent { table = \"parents\"; string name; }"},
        {"child.gnr","app.child","import app.parent; model Child { int parent_id; parent() { return belongsTo<Parent>(\"parent_id\"); } }"},
        {"route.gnr","routes.web","import app.child; controller Home { show(Child child) { return json(child); } } Route::get(\"/children/{child}\", Home::show);"}};
    const auto model_project = language::Compiler{}.compile_sources(model_modules,options);
    CHECK(model_project.success() && model_project.validated->routes().size() == 1);
    auto ir = language::CppIrLowerer{}.lower(*selected.validated,false);
    CHECK(ir.routes.size() == 1 && language::CppIrVerifier{}.verify(ir).success());
    ir.routes.front().controller.spelling.clear();
    CHECK(!language::CppIrVerifier{}.verify(ir).success());
    ir = language::CppIrLowerer{}.lower(*selected.validated,false);
    for (auto& statement : ir.statements) if (statement.kind == language::CppIrStatementKind::route_registration) statement.route = ir.routes.size();
    CHECK(!language::CppIrVerifier{}.verify(ir).success());
    std::ifstream source_file{GUNGNIR_ROUTING_FIXTURE};
    const String source{std::istreambuf_iterator<char>{source_file},{}};
    const auto compiled = language::Compiler{}.compile(source,"routing.gnr",options);
    CHECK(compiled.success() && compiled.validated->routes().size() == 18);
    const auto lowered = language::CppIrLowerer{}.lower(*compiled.validated,false);
    CHECK(lowered.routes.size() == 18 && language::CppIrVerifier{}.verify(lowered).success());
    CHECK(std::any_of(lowered.routes.begin(),lowered.routes.end(),[](const auto& route) {
        return route.action == "project" && route.parameters.size() == 1 && route.parameters[0].name == "project" && route.parameters[0].binding == language::CppIrRouteBinding::model;
    }));

    stage("bootstrap");
    Application app;
    auto trace = std::make_shared<RoutingTrace>("",0); app.container().instance<RoutingTrace>(trace);
    bind<RoutingController>(app); bind<RoutingGlobal>(app); bind<RoutingOuter>(app); bind<RoutingInner>(app); bind<RoutingLeaf>(app);
    app.middleware<RoutingGlobal>();
    app.middleware_alias<RoutingInner>("inside");
#ifdef GUNGNIR_WITH_SQLITE
    database::Settings settings; settings.backend = database::Backend::sqlite; settings.database = ":memory:";
    for (const String name : {"default","archive"}) app.database(name,settings.backend,[settings]{return std::make_shared<database::SQLiteDriver>(settings);},1);
#endif
    app.boot();
    gnr_register_routes(app);
    CHECK(app.router().route_count() == 17 && app.router().has("api.v1.project"));
    CHECK(app.router().url("api.v1.project",{{"project","42"}}) == "/api/v1/projects/42");
    CHECK(send(app.router(),"/").body() == "home");
    for (const auto method : {http::Method::post,http::Method::put,http::Method::patch,http::Method::delete_,http::Method::options,http::Method::head}) {
        const auto response = send(app.router(),"/verb",method);
        CHECK(response.status() == 200 && response.body() == http::to_string(method));
    }
    CHECK(send(app.router(),"/verb").status() == 404);
    CHECK(send(app.router(),"/unknown").body() == "fallback");
    CHECK(send(app.router(),"/letters/abc").body() == "abc");
    CHECK(send(app.router(),"/letters/123").status() == 404);
    CHECK(send(app.router(),"/uuid/550e8400-e29b-41d4-a716-446655440000").status() == 200);
    CHECK(send(app.router(),"/uuid/invalid").status() == 404);
    const auto scalar = send(app.router(),"/scalar/true/18446744073709551615/-42/3.5");
    CHECK(scalar.status() == 200);
    const auto values = Json::parse(scalar.body());
    CHECK(*values.get("id") == Json{Int64{-42}} && *values.get("enabled") == Json{true});
    CHECK(*values.get("wide") == Json{std::numeric_limits<UInt64>::max()});
    for (const auto path : {"/scalar/maybe/1/2/3","/scalar/true/-1/2/3","/scalar/true/18446744073709551616/2/3","/scalar/true/1/9223372036854775808/3","/scalar/true/1/2/nan","/scalar/true/1/2/inf","/scalar/true/1/2/3junk"}) CHECK(send(app.router(),path).status() == 404);
    CHECK(send(app.router(),"/url/42").body() == "/api/v1/projects/42");
    CHECK(send(app.router(),"/known").body() == "true");

    stage("middleware order");
    trace->reset();
    const auto ordered = send(app.router(),"/api/v1/ordered/7");
    CHECK(ordered.status() == 200);
    CHECK(ordered.header("X-Route-Trace") == "global-before|outer-before|inner-before|leaf-before|action|leaf-after|inner-after|outer-after|");
    CHECK(ordered.header("X-Global-Trace") == trace->value() && trace->value().ends_with("outer-after|global-after|"));
    trace->reset(); CHECK(send(app.router(),"/api/v1/ordered/not-a-number").status() == 404 && trace->value() == "global-before|global-after|");
    CHECK(send(app.router(),"/api/v1/ordered/%37").status() == 200);

#ifdef GUNGNIR_WITH_SQLITE
    stage("model binding");
    app.database().connection()->execute("CREATE TABLE routing_projects (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, secret TEXT, created_at TEXT, updated_at TEXT, deleted_at TEXT)");
    app.database().connection("archive")->execute("CREATE TABLE routing_documents (slug TEXT PRIMARY KEY, body TEXT)");
    const auto project = createRoutingProject("Gungnir");
    createRoutingDocument("brief-001");
    const String quoted_key = "quote' OR 1=1 --";
    createRoutingDocument(quoted_key);
    std::vector<orm::QueryEvent> queries; orm::listen([&](const auto& query){queries.push_back(query);});
    const auto shown = send(app.router(),"/api/v1/projects/" + std::to_string(project.id.get()));
    CHECK(shown.status() == 200 && queries.size() == 1 && queries.front().binding_count == 1);
    const auto payload = Json::parse(shown.body()).get("project")->as_object();
    CHECK(payload.at("name").string() == "Gungnir" && !payload.contains("secret"));
    queries.clear(); CHECK(send(app.router(),"/api/v1/documents/brief-001").status() == 200 && queries.size() == 1);
    queries.clear();
    const auto quoted = send(app.router(),app.router().url("api.v1.document",{{"document",quoted_key}}));
    CHECK(quoted.status() == 200 && Json::parse(quoted.body()).get("slug")->string() == quoted_key);
    CHECK(queries.size() == 1 && queries.front().connection == "archive" && queries.front().binding_count == 1 && queries.front().statement.find(quoted_key) == String::npos);
    const auto calls = trace->calls;
    CHECK(send(app.router(),"/api/v1/projects/999999").status() == 404 && trace->calls == calls);
    CHECK(send(app.router(),"/api/v1/projects/not-an-integer").status() == 404 && trace->calls == calls);
    CHECK(deleteRoutingProject(project.id.get()));
    CHECK(send(app.router(),"/api/v1/projects/" + std::to_string(project.id.get())).status() == 404 && trace->calls == calls);
    CHECK(send(app.router(),"/api/v1/documents/missing").status() == 404 && trace->calls == calls);
    orm::stop_listening();
#endif
    stage("URL parameter round trip");
    const String value = "a/b ?#%+{value}";
    const auto url = app.router().url("echo",{{"value",value}});
    CHECK(url == "/echo/a%2Fb%20%3F%23%25%2B%7Bvalue%7D");
    CHECK(send(app.router(),url).body() == value);
    CHECK(send(app.router(),"/echo/%252F").body() == "%2F");
    CHECK(send(app.router(),"/echo/a+b").body() == "a+b");
    CHECK(send(app.router(),"/echo/%E2%9C%93").body() == "\xE2\x9C\x93");
    for (const auto path : {"/echo/%","/echo/%0G","/echo/%00","/echo/%0D%0A"}) CHECK(send(app.router(),path).status() == 404);
    const auto escaped = app.router().url("echo",{{"value","{project}"}});
    CHECK(escaped == "/echo/%7Bproject%7D");
    CHECK(send(app.router(),"/echo-url/a%2Fb").body() == "/echo/a%2Fb");
    for (const String value : {String{},String{"\0",1},String{"\r\n"}}) {
        bool failed = false;
        try { (void)app.router().url("echo",{{"value",value}}); } catch (const std::invalid_argument&) { failed = true; }
        CHECK(failed);
    }
    stage("complete");
}
int main() {
    try { exercise_routing(); }
    catch (const std::exception& error) { std::cerr << "Unhandled routing test exception: " << error.what() << std::endl; return EXIT_FAILURE; }
}
