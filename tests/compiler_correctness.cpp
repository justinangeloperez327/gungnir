#include <gungnir/language/compiler.hpp>

#include <algorithm>
#include <cassert>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace gungnir::language;

namespace {

CompilerOptions deterministic_options() {
    CompilerOptions options;
    options.emit_line_directives = false;
    return options;
}

void assert_diagnostic_location(const CompilationResult& result, std::string_view file) {
    assert(!result.diagnostics.empty());
    for (const auto& diagnostic : result.diagnostics) {
        assert(diagnostic.location.file == file);
        assert(diagnostic.location.line >= 1);
        assert(diagnostic.location.column >= 1);
        assert(!diagnostic.code.empty());
    }
}

void parser_and_validated_ast_invariants() {
    constexpr std::string_view source =
        "function int add(int a, int b) { return a + b; } "
        "function int answer() { return add(20, 22); }";

    const auto parsed = SyntaxParser{}.parse(source, "correctness.gnr");
    assert(parsed.diagnostics.empty());
    assert(parsed.project.declarations.size() == 2);

    for (const auto& declaration : parsed.project.declarations) {
        assert(declaration.origin.end > declaration.origin.begin);
        assert(declaration.origin.line >= 1);
        assert(declaration.origin.column >= 1);
    }
    for (const auto& expression : parsed.project.expressions) {
        assert(expression.origin.end >= expression.origin.begin);
        assert(expression.origin.line >= 1);
        assert(expression.origin.column >= 1);
    }

    auto validated = ProgramValidator{}.validate(parsed.project);
    assert(validated.project.has_value());
    assert(validated.diagnostics.empty());

    for (const auto& expression : validated.project->expressions()) {
        assert(expression.type != invalid_id);
        assert(expression.type < validated.project->types().size());
        if (expression.symbol != invalid_id) {
            assert(expression.symbol < validated.project->symbols().size());
        }
    }

    for (const auto& symbol : validated.project->symbols()) {
        assert(symbol.type != invalid_id);
        assert(symbol.type < validated.project->types().size());
        if (symbol.owner != invalid_id) {
            assert(symbol.owner < validated.project->symbols().size());
        }
    }

    const auto code = CppEmitter{}.emit(*validated.project, false);
    assert(!code.empty());
}

void invalid_program_never_reaches_codegen() {
    const auto result = Compiler{}.compile(
        "function int broken() { return \"not an int\"; }",
        "type-error.gnr",
        deterministic_options());

    assert(!result.success());
    assert(!result.validated.has_value());
    assert(result.code.empty());
    assert_diagnostic_location(result, "type-error.gnr");
}

void compilation_is_deterministic() {
    constexpr std::string_view source =
        "function int square(int value) { return value * value; } "
        "function int answer() { return square(6); }";

    const auto options = deterministic_options();
    const auto first = Compiler{}.compile(source, "deterministic.gnr", options);
    const auto second = Compiler{}.compile(source, "deterministic.gnr", options);

    assert(first.success());
    assert(second.success());
    assert(first.code == second.code);
    assert(dump_validated(*first.validated) == dump_validated(*second.validated));
}

void multi_file_order_is_deterministic() {
    const SourceFile math{
        "math.gnr",
        "math",
        "module math; export function int add(int a, int b) { return a + b; }"};
    const SourceFile main{
        "main.gnr",
        "main",
        "module main; import math as Math; function int answer() { return Math::add(20, 22); }"};

    const auto options = deterministic_options();
    const auto forward = Compiler{}.compile_sources({math, main}, options);
    const auto reverse = Compiler{}.compile_sources({main, math}, options);

    assert(forward.success());
    assert(reverse.success());
    assert(forward.code == reverse.code);
    assert(dump_validated(*forward.validated) == dump_validated(*reverse.validated));
}

void malformed_input_is_controlled() {
    const std::vector<std::string> malformed{
        "function",
        "function int missing( {",
        "function int f() { return \"unterminated; }",
        "controller {",
        "model User { string name;"
    };

    for (std::size_t i = 0; i < malformed.size(); ++i) {
        const auto file = "malformed-" + std::to_string(i) + ".gnr";
        const auto result = Compiler{}.compile(malformed[i], file, deterministic_options());
        assert(!result.success());
        assert(!result.validated.has_value());
        assert(result.code.empty());
        assert_diagnostic_location(result, file);
    }
}

void framework_contracts_fail_before_codegen() {
    const auto result = Compiler{}.compile(
        "event BrokenEvent { int id; public handle() {} }",
        "event.gnr",
        deterministic_options());

    assert(!result.success());
    assert(!result.validated.has_value());
    assert(result.code.empty());
    assert_diagnostic_location(result, "event.gnr");
}

} // namespace

int main() {
    parser_and_validated_ast_invariants();
    invalid_program_never_reaches_codegen();
    compilation_is_deterministic();
    multi_file_order_is_deterministic();
    malformed_input_is_controlled();
    framework_contracts_fail_before_codegen();
}
