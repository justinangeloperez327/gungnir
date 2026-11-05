#include "structured_errors_program.cpp"
#include <gungnir/language/compiler.hpp>
#include <gungnir/language/cpp_ir.hpp>
#include <gungnir/logging/memory_sink.hpp>
#ifdef GUNGNIR_WITH_SQLITE
#include <gungnir/database/sqlite.hpp>
#endif
#include <cassert>
#include <iostream>
using namespace gungnir;
namespace {
template<class T> T wait(Task<T> task) { task.run_inline(); return task.operator co_await().await_resume(); }
void check_invalid(std::string_view source) {
    for (const bool validate_only : {false,true}) {
        language::CompilerOptions options; options.validate_only = validate_only;
        const auto result = language::Compiler{}.compile(source,"invalid-errors.gnr",options);
        assert(!result.success() && result.code.empty());
        assert(std::any_of(result.diagnostics.begin(),result.diagnostics.end(),[](const auto& diagnostic){ return diagnostic.code == "GNR2380"; }));
    }
}
}
int main() {
    assert(catchMessage() == "caught message" && catchStatus() == 409);
    assert(normalTryReturn() == 42 && normalTryFallthrough() == 42);
    assert(rethrowMessage() == "original" && catchInArrow() == "arrow" && loopInTry() == 2);
    assert(wait(catchAfterAwait()) == "async failure" && wait(rethrowAfterAwait()) == "async failure");
    for (const auto source : {
        "function void f() { throw; }",
        "function void f() { try {} }",
        "function void f() { try {} catch (string error) {} }",
        "function void f() { try {} catch (Error? error) {} }",
        "function void f() { try {} catch (Error error) {} catch (HttpError other) {} }",
        "function void f() { try {} catch (HttpError error) {} catch (HttpError other) {} }",
        "function void f() { try {} catch (Error error) { const run = () => { throw; }; } }",
        "function void f() { abort(200); }",
        "function void f() { throw 1; }",
        "function void f() { let error = Error; }",
        "function void f() { let error = ValidationError(); }",
        "controller C { Error failure; index() { return noContent(); } }"
    }) check_invalid(source);
    for (const auto source : {
        "function string f() { let value: string? = 'start'; if (value == null) { return ''; } try { value = null; throw 'error'; } catch (Error error) { return value; } }",
        "function void f() { try {} catch (Error error) { error = Error('changed'); } }",
        "function void f() { try {} catch (Error error) {} error.message(); }"
    }) assert(!language::Compiler{}.compile(source).success());
    const auto compiled = language::Compiler{}.compile("function int f() { try { return 1; } catch (Error error) { return 2; } }");
    assert(compiled.success());
    auto ir = language::CppIrLowerer{}.lower(*compiled.validated,false);
    assert(language::CppIrVerifier{}.verify(ir).success());
    const auto try_id = ir.functions.front().body.front();
    ir.statements[try_id].alternative.clear();
    assert(!language::CppIrVerifier{}.verify(ir).success());

    Application app;
    auto logs = std::make_shared<logging::MemorySink>();
    auto logger = std::make_shared<logging::Logger>(); logger->sink(logs);
    app.exceptions().logger(logger);
    app.bind<ErrorController>();
#ifdef GUNGNIR_WITH_SQLITE
    database::Settings settings; settings.backend = database::Backend::sqlite; settings.database = ":memory:";
    app.database("default",settings.backend,[settings]{return std::make_shared<database::SQLiteDriver>(settings);},1);
#endif
    app.boot(); gnr_register_routes(app);
    Request request{http::Method::get,"/boom"}; request.set_header("Accept","application/json");
    auto response = wait(app.router().dispatch(request));
    assert(response.status() == 500 && response.body().find("private failure") == String::npos);
    assert(logs->records().size() == 1);
    app.config().set("app.debug",true); app.config().set("app.env","local");
    response = wait(app.router().dispatch(request));
    const auto error = Json::parse(response.body());
    assert(error.get("message")->string() == "private failure");
    assert(error.get("file")->string().ends_with("errors.gnr") && *error.get("line") == Json{Int64{63}});
    // The source location must refer to Gungnir even with native #line directives disabled.
    Request asynchronous{http::Method::get,"/async"}; asynchronous.set_header("Accept","application/json");
    assert(wait(app.router().dispatch(asynchronous)).body().find("private async failure") != String::npos);
    Request conflict{http::Method::get,"/conflict"}; assert(wait(app.router().dispatch(conflict)).status() == 409);
    Request validation{http::Method::post,"/validation"}; validation.set_header("Accept","application/json");
    response = wait(app.router().dispatch(validation));
    assert(response.status() == 422 && Json::parse(response.body()).get("errors")->get("name"));
    Request handled{http::Method::get,"/handled"};
    response = wait(app.router().dispatch(handled));
    assert(response.status() == 200 && response.body() == "recovered" && logs->records().back().message == "handled failure");
#ifdef GUNGNIR_WITH_SQLITE
    app.database().connection()->execute("CREATE TABLE error_users (id INTEGER PRIMARY KEY, name TEXT)");
    Request missing{http::Method::get,"/users/123"}; assert(wait(app.router().dispatch(missing)).status() == 404);
    Request caught{http::Method::get,"/caught/123"}; response = wait(app.router().dispatch(caught));
    assert(response.status() == 404 && response.body() == "caught missing model");
#endif
    std::cout << "Structured error handling, async recovery and HTTP tests passed\n";
}
