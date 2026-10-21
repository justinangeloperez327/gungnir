#include <gungnir/language/compiler.hpp>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace gungnir::language;
void reject(std::string_view source, std::string_view code = {}) {
    const auto result = Compiler{}.compile(source,"failure.gnr"); assert(!result.success() && result.code.empty() && !result.validated);
    if (!code.empty()) { bool found = false; for (const auto& item : result.diagnostics) { assert(item.location.file == "failure.gnr"); if (item.code == code) found = true; } if (!found) { for (auto& d : result.diagnostics) std::cerr << d.code << ' ' << d.message << '\n'; } assert(found); }
}
int main() {
    const auto parsed = SyntaxParser{}.parse("function int answer() { return 42; }"); assert(parsed.diagnostics.empty() && parsed.project.statements.size() == 1);
    assert(parsed.project.declarations.front().origin.begin == 0);
    assert(parsed.project.declarations.front().origin.end == 36);
    assert(parsed.project.expressions.front().origin.end > parsed.project.expressions.front().origin.begin);
    auto validated = ProgramValidator{}.validate(parsed.project); assert(validated.project && validated.diagnostics.empty()); assert(!CppEmitter{}.emit(*validated.project).empty());
    reject("function int broken() { const x = 1; }","GNR2215");
    reject("function int broken() { return absent; }","GNR2202");
    reject("function int broken() { const x = 1; x = 2; return x; }","GNR2208");
    reject("controller Counter { int count; } function int broken() { const counter = Counter(0); counter.count = 2; return counter.count; }", "GNR2208");
    reject("controller Counter { List<int> values; } function int broken() { const counter = Counter([1]); counter.values[0] = 2; return counter.values[0]; }", "GNR2208");
    reject("controller Counter { int count; } controller Holder { Counter counter; } function int broken() { const holder = Holder(Counter(0)); holder.counter.count++; return holder.counter.count; }", "GNR2208");
    reject("controller Counter { int count; int increment() { count += 1; return count; } } function int broken() { const counter = Counter(0); return counter.increment(); }", "GNR2208");
    reject("controller Counter { int count; int number() { return count; } } function int broken() { let counter = Counter(0); const run = () => counter.number(); return run(); }", "GNR2208");
    reject("controller Counter { int count; } function int broken() { let counter = Counter(0); const run = () => { counter.count = 2; return counter.count; }; return run(); }", "GNR2208");
    reject("function int broken() { let value = 1; const run = () => { value = 2; return value; }; return run(); }", "GNR2208");
    reject("function int broken() { let values = [1]; const run = () => { values[0]++; return values[0]; }; return run(); }", "GNR2208");
    reject("function int broken() { const run = () => { let value = 0; const inner = () => { value++; return value; }; return inner(); }; return run(); }", "GNR2208");
    reject("function int broken(List<int>? values) { for (value in values) { return value; } return 0; }");
    reject("function string broken(Map<string,int>? values) { for (key in values) { return key; } return ''; }");
    reject("function int broken() { const run = (int value) => value; for (value in run) { return value; } return 0; }");
    reject("function int f(int x) { return x; } function int g() { return f(x: 1, x: 2); }","GNR2209");
    reject("async function int f() { return 1; } function int g() { return f(); }","GNR2216");
    reject("function int g() { return await 1; }","GNR2210");
    reject("model A { B value; } model B { A value; }", "GNR2217");
    reject("function int f() { return [1].filter((item) => item); }");
    reject("migration M { up() { Table::create(2, (table) => {}); } down() {} }");
    reject("event E { int id; handle() {} }");
    reject("migration M { up() {} }");
    reject("model M { casts = { name: 'unsupported' }; }");
    reject("function int f() { return 99999999999999999999999; }","GNR2003");
    reject("function string f() { return '\\uD800'; }","GNR2002");
    reject("controller C { index() { return text(1); } }");
    reject("mail M { int subject() { return 1; } }");
    reject("notification N { int via(int recipient) { return 1; } }");
    reject("function int f(void a) { return 1; }");
    reject("function List<void> f() { return []; }");
    reject("function int f() { return [1].map((x) => {}).size(); }");
    reject("function bool f() { const x = {a: 1}; return x == x; }");
    reject("function int f() { for (let i = 0; i < 1; i = a) { const a = 1; } return 1; }");
    reject("controller C { int f() { return 1; } } function int f() { return C::f(); }");
    const auto valid = Compiler{}.compile("function int f() { const x = 2; const fn = (int item) => { const local = item + x; return local; }; return fn(3); }"); assert(valid.success());
    for (const auto& expression : valid.validated->expressions()) assert(expression.type != invalid_id);

    const auto conditional_widening = Compiler{}.compile(
        "function double choose(bool flag) { return flag ? 1 : 2.5; }"
    );
    assert(conditional_widening.success());

    const auto nullable_conditional = Compiler{}.compile(
        "function string? choose(bool flag) { return flag ? null : 'value'; }"
    );
    assert(nullable_conditional.success());

    const auto exhaustive_branch_flow = Compiler{}.compile(
        "function int choose(bool flag) { "
        "if (flag) { return 1; } else { return 2; } }"
    );
    assert(exhaustive_branch_flow.success());

    const auto return_or_throw_flow = Compiler{}.compile(
        "function int choose(bool flag) { "
        "if (flag) { return 1; } else { throw 'failed'; } }"
    );
    assert(return_or_throw_flow.success());

    reject(
        "function int loopOnly(bool flag) { "
        "while (flag) { return 1; } }",
        "GNR2215"
    );

    const auto terminating_null_guard = Compiler{}.compile(
        "function string require(string? value) { "
        "if (value == null) { return 'fallback'; } "
        "return value; }"
    );
    assert(terminating_null_guard.success());

    const auto terminating_else_guard = Compiler{}.compile(
        "function string require(string? value) { "
        "if (value != null) { const copy = value; } "
        "else { return 'fallback'; } "
        "return value; }"
    );
    assert(terminating_else_guard.success());

    reject(
        "function string invalid(string? input) { "
        "let value = input; "
        "if (value != null) { value = null; return value; } "
        "return 'fallback'; }",
        "GNR2214"
    );

    reject(
        "function string invalidContinuation(string? input) { "
        "let value = input; "
        "if (value != null) { value = null; } "
        "else { return 'fallback'; } "
        "return value; }",
        "GNR2214"
    );

    reject(
        "function int mixed(int left, uint64 right) { return left + right; }"
    );
    reject(
        "function double unsafe(uint64 value) { return value; }",
        "GNR2214"
    );
    reject(
        "function decimal unsafe(double value) { return value; }",
        "GNR2214"
    );

    const auto decimal_math = Compiler{}.compile(
        "function decimal total(decimal left, decimal right) { return left + right; }"
    );
    assert(decimal_math.success());
    bool saw_decimal_add = false;
    for (std::size_t i = 0; i < decimal_math.validated->syntax().expressions.size(); ++i) {
        const auto& syntax = decimal_math.validated->syntax().expressions[i];
        if (syntax.kind == SyntaxExpressionKind::binary && syntax.text == "+") {
            assert(decimal_math.validated->types()[decimal_math.validated->expressions()[i].type].name == "decimal");
            saw_decimal_add = true;
        }
    }
    assert(saw_decimal_add);
    const auto root = std::filesystem::temp_directory_path()/"gungnir-structured-modules"; std::filesystem::remove_all(root); std::filesystem::create_directories(root);
    auto write = [&](const char* path,const char* source) { std::ofstream{root/path} << source; };
    write("math.gnr","module math; export function int add(int a,int b) { return a+b; }");
    write("main.gnr","module main; import math as Math; function int result() { return Math::add(2,3); }");
    auto project = Compiler{}.compile_project(root); if (!project.success()) for (auto& d : project.diagnostics) std::cerr << d.code << ' ' << d.message << '\n'; assert(project.success()); assert(project.validated->module_order().size() == 2);
    write("extra.gnr","module extra; function int add(int a,int b) { return a+b; }");
    write("main.gnr","module main; import math; import extra; function int result() { return add(2,3); }"); assert(!Compiler{}.compile_project(root).success());
    std::filesystem::remove(root/"extra.gnr");
    write("main.gnr","module main; import math as Math; function int result() { return Math::add(2,3); }");
    write("math.gnr","module math; import main; function int add(int a,int b) { return a+b; }"); assert(!Compiler{}.compile_project(root).success());
    write("math.gnr","module wrong; function int add(int a,int b) { return a+b; }"); assert(!Compiler{}.compile_project(root).success());
    write("main.gnr","module main; import missing; function int result() { return 1; }"); assert(!Compiler{}.compile_project(root).success());
    std::filesystem::remove_all(root);
}
