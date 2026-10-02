#include <gungnir/language/compiler.hpp>
#include <gungnir/language/cpp_ir.hpp>
#include <gungnir/language/diagnostic_renderer.hpp>

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

void cpp_ir_is_an_explicit_deterministic_boundary() {
    constexpr std::string_view source =
        "function int add(int a, int b) { return a + b; } "
        "function int answer() { return add(20, 22); }";

    auto parsed = SyntaxParser{}.parse(source, "ir.gnr", "app");
    assert(parsed.diagnostics.empty());
    auto validation = ProgramValidator{}.validate(std::move(parsed.project));
    assert(validation.project.has_value());
    assert(validation.diagnostics.empty());

    const auto first = CppIrLowerer{}.lower(*validation.project, false);
    const auto second = CppIrLowerer{}.lower(*validation.project, false);

    assert(!first.interface_declarations.empty());
    assert(
        first.interface_declarations.size() ==
        first.header_declarations.size());
    assert(
        first.interface_declarations.front().kind ==
        CppIrDeclarationKind::preamble);
    assert(!first.expressions.empty());
    assert(!first.statements.empty());
    assert(first.functions.size() == 2);
    assert(first.units.size() == validation.project->module_order().size());
    assert(first.units.size() == 1);
    assert(first.units.front().module == "app");
    assert(first.units.front().functions.size() == first.functions.size());

    const auto verification = CppIrVerifier{}.verify(first);
    assert(verification.success());
    assert(verification.errors.empty());

    bool saw_call = false;
    bool saw_binary = false;
    bool saw_return = false;

    for (const auto& expression : first.expressions) {
        assert(expression.type.valid());
        assert(!expression.spelling.empty());
        saw_call = saw_call ||
            expression.kind == CppIrExpressionKind::call;
        saw_binary = saw_binary ||
            expression.kind == CppIrExpressionKind::binary;
    }

    for (const auto& statement : first.statements) {
        saw_return = saw_return ||
            statement.kind == CppIrStatementKind::return_;
    }

    assert(saw_call);
    assert(saw_binary);
    assert(saw_return);

    for (const auto& function : first.functions) {
        assert(!function.name.empty());
        assert(function.result.valid());
        assert(!function.body.empty());
        assert(!function.coroutine);
    }

    const auto ir_dump = dump_cpp_ir(first);
    assert(ir_dump.starts_with("cpp-ir structural\n"));
    assert(ir_dump.find("function") != std::string::npos);
    assert(ir_dump.find("expression") != std::string::npos);
    assert(ir_dump.find("unit app") != std::string::npos);
    assert(ir_dump == dump_cpp_ir(second));

    const auto direct = CppEmitter{}.emit(first);
    const auto compatibility_wrapper =
        CppEmitter{}.emit(*validation.project, false);
    assert(direct == compatibility_wrapper);

    const auto emitted_units = CppEmitter{}.emit_units(first);
    assert(emitted_units.units.size() == first.units.size());
    assert(emitted_units.declarations.starts_with("#pragma once\n"));
    assert(emitted_units.units.front().code.starts_with(
        "#include \"program.hpp\"\n"));

    auto invalid_declaration = first;
    invalid_declaration.interface_declarations.front().spelling.clear();
    assert(!CppIrVerifier{}.verify(invalid_declaration).success());

    auto invalid_unit = first;
    invalid_unit.units.front().functions.push_back(999999);
    assert(!CppIrVerifier{}.verify(invalid_unit).success());

    auto invalid_coroutine = first;
    const auto first_statement =
        invalid_coroutine.functions.front().body.front();
    invalid_coroutine.statements[first_statement].kind =
        CppIrStatementKind::co_return_;
    assert(!CppIrVerifier{}.verify(invalid_coroutine).success());

    auto invalid_type = first;
    invalid_type.expressions.front().type.spelling.clear();
    assert(!CppIrVerifier{}.verify(invalid_type).success());
}

void cpp_ir_recursive_lowering_survives_arena_growth() {
    std::string source{"function int stress() { return "};
    for (int i = 0; i < 96; ++i) {
        source += "(";
    }
    source += "1";
    for (int i = 0; i < 96; ++i) {
        source += " + 1)";
    }
    source += "; }";

    auto parsed = SyntaxParser{}.parse(
        source,
        "ir-arena-growth.gnr",
        "app"
    );
    assert(parsed.diagnostics.empty());

    auto validation =
        ProgramValidator{}.validate(std::move(parsed.project));
    assert(validation.project.has_value());
    assert(validation.diagnostics.empty());

    const auto ir =
        CppIrLowerer{}.lower(*validation.project, false);
    const auto verification = CppIrVerifier{}.verify(ir);

    assert(verification.success());
    assert(ir.expressions.size() > 96);
    assert(!dump_cpp_ir(ir).empty());
}

void validation_only_stops_at_semantic_firewall() {
    auto options = deterministic_options();
    options.validate_only = true;

    const auto result = Compiler{}.compile(
        "function int answer() { return 42; }",
        "check-only.gnr",
        options
    );

    assert(result.success());
    assert(result.validated.has_value());
    assert(result.code.empty());

    const auto ir = CppIrLowerer{}.lower(*result.validated, false);
    assert(CppIrVerifier{}.verify(ir).success());
}

void authoritative_check_closes_control_flow() {
    auto options = deterministic_options();
    options.validate_only = true;

    const std::vector<std::string_view> valid{
        "function int choose(bool flag) { if (flag) { return 1; } else { return 2; } }",
        "function int choose(bool flag) { if (flag) { return 1; } else { throw 'failed'; } }",
        "function int nested(bool first, bool second) { "
            "if (first) { if (second) { return 1; } else { return 2; } } "
            "else { return 3; } }",
        "function string require(string? value) { "
            "if (value == null) { throw 'missing'; } "
            "return value; }"
    };

    for (std::size_t i = 0; i < valid.size(); ++i) {
        const auto file = "flow-valid-" + std::to_string(i) + ".gnr";
        const auto result = Compiler{}.compile(valid[i], file, options);
        assert(result.success());
        assert(result.validated.has_value());
        assert(result.code.empty());
    }

    struct InvalidCase {
        std::string_view source;
        std::string_view code;
    };
    const std::vector<InvalidCase> invalid{
        {
            "function int missing(bool flag) { if (flag) { return 1; } }",
            "GNR2215"
        },
        {
            "function int loop(bool flag) { while (flag) { return 1; } }",
            "GNR2215"
        },
        {
            "function int broken() { break; }",
            "GNR2201"
        },
        {
            "function int broken() { continue; }",
            "GNR2201"
        }
    };

    for (std::size_t i = 0; i < invalid.size(); ++i) {
        const auto file = "flow-invalid-" + std::to_string(i) + ".gnr";
        const auto result = Compiler{}.compile(invalid[i].source, file, options);
        assert(!result.success());
        assert(!result.validated.has_value());
        assert(result.code.empty());
        assert_diagnostic_location(result, file);
        assert(std::any_of(
            result.diagnostics.begin(),
            result.diagnostics.end(),
            [&](const auto& diagnostic) {
                return diagnostic.code == invalid[i].code;
            }
        ));
    }
}

void authoritative_check_matches_full_semantic_gate() {
    const std::vector<std::string_view> corpus{
        "function int answer() { return 42; }",
        "function int broken() { return 'wrong'; }",
        "function int missing(bool flag) { if (flag) { return 1; } }",
        "function string require(string? value) { "
            "if (value == null) { return 'fallback'; } return value; }",
        "async function int load() { return 1; } function int use() { return load(); }",
        "event BrokenEvent { int id; public handle() {} }"
    };

    for (std::size_t i = 0; i < corpus.size(); ++i) {
        auto check_options = deterministic_options();
        check_options.validate_only = true;
        const auto file = "semantic-parity-" + std::to_string(i) + ".gnr";
        const auto checked = Compiler{}.compile(corpus[i], file, check_options);
        const auto compiled = Compiler{}.compile(
            corpus[i],
            file,
            deterministic_options()
        );

        assert(checked.success() == compiled.success());
        assert(checked.validated.has_value() == compiled.validated.has_value());
        assert(checked.code.empty());

        std::vector<std::string> checked_codes;
        std::vector<std::string> compiled_codes;
        for (const auto& diagnostic : checked.diagnostics) {
            checked_codes.push_back(diagnostic.code);
        }
        for (const auto& diagnostic : compiled.diagnostics) {
            compiled_codes.push_back(diagnostic.code);
        }
        assert(checked_codes == compiled_codes);
    }
}

void diagnostics_preserve_precise_source_spans() {
    auto options = deterministic_options();
    options.validate_only = true;

    const std::string source =
        "function int broken() {\n"
        "    return absent;\n"
        "}\n";
    const auto result = Compiler{}.compile(
        source,
        "diagnostic-span.gnr",
        options
    );

    assert(!result.success());
    const auto found = std::find_if(
        result.diagnostics.begin(),
        result.diagnostics.end(),
        [](const auto& diagnostic) {
            return diagnostic.code == "GNR2202";
        }
    );
    assert(found != result.diagnostics.end());
    assert(found->location.line == 2);
    assert(found->location.column >= 1);
    assert(found->span.valid);
    assert(found->span.end_line == 2);
    assert(found->span.end_column > found->location.column);
    assert(found->span.end_offset > found->span.begin_offset);
    assert(found->source_line == "    return absent;");

    const auto rendered = DiagnosticRenderer::render(*found);
    assert(rendered.find("error[GNR2202]") != std::string::npos);
    assert(rendered.find("2 |     return absent;") != std::string::npos);
    assert(rendered.find("^~~~~~") != std::string::npos);
}

void diagnostic_order_is_deterministic() {
    const SourceFile z{
        "z.gnr",
        "z",
        "function int zed() { return missing_z; }"
    };
    const SourceFile a{
        "a.gnr",
        "a",
        "function int alpha() { return missing_a; }"
    };

    auto options = deterministic_options();
    options.validate_only = true;
    const auto first = Compiler{}.compile_sources({z, a}, options);
    const auto second = Compiler{}.compile_sources({a, z}, options);

    assert(!first.success());
    assert(!second.success());
    assert(first.diagnostics.size() == second.diagnostics.size());
    assert(first.diagnostics.size() >= 2);

    for (std::size_t i = 0; i < first.diagnostics.size(); ++i) {
        assert(first.diagnostics[i].location.file ==
               second.diagnostics[i].location.file);
        assert(first.diagnostics[i].location.line ==
               second.diagnostics[i].location.line);
        assert(first.diagnostics[i].location.column ==
               second.diagnostics[i].location.column);
        assert(first.diagnostics[i].code ==
               second.diagnostics[i].code);
        assert(first.diagnostics[i].message ==
               second.diagnostics[i].message);
    }
    assert(first.diagnostics.front().location.file == "a.gnr");
}

void generated_cpp_has_statement_source_mapping() {
    CompilerOptions options;
    options.emit_line_directives = true;
    const auto result = Compiler{}.compile(
        "function int answer() {\n"
        "    const value = 42;\n"
        "    return value;\n"
        "}\n",
        "source-map.gnr",
        options
    );

    assert(result.success());
    assert(
        result.code.find("#line 1 \"source-map.gnr\"") !=
        std::string::npos
    );
    assert(
        result.code.find("#line 2 \"source-map.gnr\"") !=
        std::string::npos
    );
    assert(
        result.code.find("#line 3 \"source-map.gnr\"") !=
        std::string::npos
    );

    options.emit_line_directives = false;
    const auto without_lines = Compiler{}.compile(
        "function int answer() { return 42; }",
        "no-lines.gnr",
        options
    );
    assert(without_lines.success());
    assert(without_lines.code.find("#line ") == std::string::npos);
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
    cpp_ir_is_an_explicit_deterministic_boundary();
    cpp_ir_recursive_lowering_survives_arena_growth();
    validation_only_stops_at_semantic_firewall();
    authoritative_check_closes_control_flow();
    authoritative_check_matches_full_semantic_gate();
    diagnostics_preserve_precise_source_spans();
    diagnostic_order_is_deterministic();
    generated_cpp_has_statement_source_mapping();
    invalid_program_never_reaches_codegen();
    compilation_is_deterministic();
    multi_file_order_is_deterministic();
    malformed_input_is_controlled();
    framework_contracts_fail_before_codegen();
}
