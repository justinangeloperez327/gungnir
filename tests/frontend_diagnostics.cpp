#include <cassert>
#include <string>
#include <vector>
#include <gungnir/language/language.hpp>

int main() {
    using namespace gungnir::language;
    const auto rejects = [](std::string source, std::string code) {
        auto result = Transpiler{}.transpile(source, "regression.gnr");
        assert(!result.success() && result.code.empty());
        bool found = false;
        for (const auto& diagnostic : result.diagnostics) {
            assert(diagnostic.location.file == "regression.gnr");
            assert(diagnostic.location.line >= 1 && diagnostic.location.column >= 1);
            found |= diagnostic.code == code;
        }
        assert(found);
    };
    rejects("/* never closed", "GNR0901");
    rejects("const value = \"never closed;", "GNR0902");
    rejects("const value = R\"tag(never closed", "GNR0902");
    rejects("const value = 1.2.3;", "GNR0903");
    rejects("const value = 12abc;", "GNR0903");
    rejects("const value = 1e+;", "GNR0903");
    rejects("const value = 'hello';", "GNR0904");
    rejects("controller Home { Response index() { return json(missing); } }", "GNR1324");
    rejects("controller Home { int index() { const count = 1; } }", "GNR1319");
    rejects("controller Home { int index() { if (true) { const x = 1; } return x; } }", "GNR1324");
    rejects("controller Home { public index() { return text(\"ok\"); } }", "GNR1330");
    rejects("controller Home { index() { return text(\"ok\"); } }", "GNR1330");
    rejects("migration CreateUsers { up() {} down() {} }", "GNR1330");
    rejects("module app.home;", "GNR1330");
    rejects("import app.user;", "GNR1330");
    rejects("function string hello() { return \"hello\"; }", "GNR1330");
    rejects("const fn = (table) => table.id();", "GNR1330");
    rejects("model User { hidden = [\"password\"]; }", "GNR1330");
    rejects("event Registered { string email; }", "GNR1330");

    for (const auto source : {
        "const value = 1e-3;", "const value = 0xDEAD;", "const value = .5;",
        "const value = 'a';", "const value = '\\n';", "const value = '\\x41';",
        "const value = R\"tag(hello)tag\";",
        "controller Home { int index() { int count = 1; return count; } }",
        "controller Home { int index() { throw std::runtime_error(\"failed\"); } }",
        "model User { posts() { return hasMany<Post>(); } }"
    }) assert(Transpiler{}.transpile(source).success());

    SemanticIndex project;
    project.closed_world = true;
    auto unknown = Transpiler{}.transpile(
        "controller Home { Missing index(Unknown value) { return external(); } }",
        "closed.gnr", {.semantic_index = &project});
    unsigned types = 0, functions = 0;
    for (const auto& diagnostic : unknown.diagnostics) {
        types += diagnostic.code == "GNR1326";
        functions += diagnostic.code == "GNR1325";
    }
    assert(types == 2 && functions == 1 && unknown.code.empty());
    project.external_types = {"Missing", "Unknown"};
    project.external_functions = {"external"};
    assert(Transpiler{}.transpile(
        "controller Home { Missing index(Unknown value) { return external(); } }",
        "closed.gnr", {.semantic_index = &project}).success());

    std::vector<Diagnostic> diagnostics;
    const auto tokens = Lexer{"\n  /* unfinished"}.tokenize(&diagnostics, "lex.gnr");
    assert(!tokens.empty() && diagnostics.size() == 1);
    assert(diagnostics[0].location.line == 2 && diagnostics[0].location.column == 3);
}
