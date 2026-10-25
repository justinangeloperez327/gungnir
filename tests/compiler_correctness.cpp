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

void structured_model_metadata_is_canonical() {
    constexpr std::string_view source =
        "model User { "
        "table = 'app_users'; "
        "connection = 'reporting'; "
        "primaryKey = 'uuid'; "
        "incrementing = false; "
        "fillable = ['name', 'email']; "
        "hidden = ['password']; "
        "visible = ['uuid', 'name', 'email']; "
        "casts = {'uuid': 'string', 'active': 'bool', 'settings': 'json'}; "
        "timestamps = true; "
        "softDeletes = true; "
        "}";

    const auto result = Compiler{}.compile(
        source,
        "model-metadata.gnr",
        deterministic_options()
    );

    assert(result.success());
    assert(result.validated.has_value());
    assert(result.code.find("gungnir::Table table{\"app_users\"}") !=
           std::string::npos);
    assert(result.code.find("gungnir::Connection connection{\"reporting\"}") !=
           std::string::npos);
    assert(result.code.find("gungnir::Fillable{\"name\",\"email\"}") !=
           std::string::npos);
    assert(result.code.find("std::string_view primaryKey = \"uuid\"") !=
           std::string::npos);
    assert(result.code.find("gungnir::PrimaryKey<gungnir::String> uuid") !=
           std::string::npos);
    assert(result.code.find("gungnir::SoftDeletes soft_deletes{}") !=
           std::string::npos);
    assert(result.code.find("gungnir::Field<bool> active") !=
           std::string::npos);
    assert(result.code.find("gungnir::Field<gungnir::Json> settings") !=
           std::string::npos);

    const auto invalid = Compiler{}.compile(
        "model User { primaryKey = 'uuid'; incrementing = true; "
        "casts = {'uuid': 'string'}; }",
        "model-metadata-invalid.gnr",
        deterministic_options()
    );

    assert(!invalid.success());
    assert(!invalid.validated.has_value());
    assert(invalid.code.empty());
    assert(std::any_of(
        invalid.diagnostics.begin(),
        invalid.diagnostics.end(),
        [](const auto& diagnostic) {
            return diagnostic.code == "GNR2308";
        }
    ));
}

void model_naming_and_cast_contracts_are_canonical() {
    const auto names = Compiler{}.compile(
        "model APIKey {} model Buzz {} model Company {}",
        "model-naming.gnr",
        deterministic_options()
    );
    assert(names.success() && names.validated);
    assert(names.code.find("gungnir::Table table{\"api_keys\"}") != std::string::npos);
    assert(names.code.find("gungnir::Table table{\"buzzes\"}") != std::string::npos);
    assert(names.code.find("gungnir::Table table{\"companies\"}") != std::string::npos);

    const auto consistent = Compiler{}.compile(
        "model Entry { "
        "bool active; int quantity; string occurred_at; "
        "casts = {'active': 'bool', 'quantity': 'integer', "
        "'occurred_at': 'datetime'}; }",
        "model-casts.gnr",
        deterministic_options()
    );
    assert(consistent.success() && consistent.validated);

    for (const auto source : {
        "model Entry { bool active; casts = {'active': 'integer'}; }",
        "model Entry { int amount; casts = {'amount': 'bool'}; }",
        "model Entry { fillable = ['name', 'name']; }",
        "model Entry { casts = {'active': 'bool', 'active': 'bool'}; }"
    }) {
        auto options = deterministic_options();
        const auto full = Compiler{}.compile(source, "invalid-model.gnr", options);
        options.validate_only = true;
        const auto check = Compiler{}.compile(source, "invalid-model.gnr", options);
        assert(!full.success() && !full.validated && full.code.empty());
        assert(!check.success() && !check.validated && check.code.empty());
        assert(std::any_of(
            full.diagnostics.begin(), full.diagnostics.end(),
            [](const auto& diagnostic) {
                return diagnostic.code == "GNR2308";
            }
        ));
        assert(full.diagnostics.size() == check.diagnostics.size());
        assert_diagnostic_location(full, "invalid-model.gnr");
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

void optional_flow_rejects_stale_nonnull_facts() {
    const std::vector<std::string_view> sources{
        "function int broken(int? input, bool reset) { let value = input; "
        "if (value == null) { return 0; } if (reset) { value = null; } return value; }",
        "function int broken(int? input, bool reset) { let value = input; "
        "if (value == null) { return 0; } while (reset) { value = null; break; } return value; }",
        "function int broken(int? input) { let value = input; const read = () => { "
        "if (value == null) { return 0; } return value; }; return value; }",
        "function int broken(int? input, bool reset) { let value = input; "
        "if (value != null) { if (reset) { value = null; } return value; } return 0; }",
        "function int broken(int? input, bool repeat) { let value = input; "
        "if (value == null) { return 0; } while (repeat) { const copy = value + 1; value = null; } return 0; }",
        "function int broken(int? input) { let value = input; "
        "if (value == null) { return 0; } for (let i = 0; i < 2; i++) { const copy = value + 1; value = null; } return 0; }",
        "function int broken(int? input) { let value = input; "
        "if (value == null) { return 0; } for (item in [1,2]) { const copy = value + 1; value = null; } return 0; }",
        "function int broken(int? input) { let value = input; "
        "if (value == null) { return 0; } while (value > 0) { value = null; } return 0; }",
        "function int broken(int? input, bool reset) { let value = input; "
        "if (value == null) { return 0; } for (let i = 0; i < 2; value++) { if (reset) { value = null; continue; } i++; } return 0; }",
        "function int broken(int? input) { let value = input; "
        "for (let i = 0; i < 2; value++) { if (value == null) { continue; } value++; } return 0; }",
        "function int broken(int? input) { let value = input; "
        "for (let i = 0; i < 0; i++) { if (value == null) { return 0; } } return value; }",
        "function int broken(int? input, List<int> items) { let value = input; "
        "for (item in items) { if (value == null) { return 0; } } return value; }",
        "function int broken(int? input) { let value = input; const outer = () => { "
        "const inner = () => { if (value == null) { return 0; } return value; }; return 0; }; return value; }"
    };
    for (const auto source : sources) {
        auto options = deterministic_options();
        const auto emitted = Compiler{}.compile(source, "null-flow.gnr", options);
        options.validate_only = true;
        const auto checked = Compiler{}.compile(source, "null-flow.gnr", options);
        assert(!emitted.success() && !checked.success());
        assert(emitted.code.empty() && checked.code.empty());
        assert(!emitted.validated && !checked.validated);
        assert(emitted.diagnostics.size() == checked.diagnostics.size());
        assert_diagnostic_location(emitted, "null-flow.gnr");
        for (std::size_t i = 0; i < emitted.diagnostics.size(); ++i) {
            assert(emitted.diagnostics[i].code == checked.diagnostics[i].code);
            assert(emitted.diagnostics[i].message == checked.diagnostics[i].message);
        }
    }
}

void optional_indexing_requires_a_nonnull_receiver() {
    const std::vector<std::string_view> sources{
        "function int broken(List<int>? values) { return values[0]; }",
        "function int broken(Map<string,int>? values) { return values['key']; }",
        "model IndexedUser { table = 'indexed_users'; } "
        "function int broken(Collection<IndexedUser>? values) { return values[0].id; }",
        "function Json broken(Json? values) { return values['key']; }",
        "function Json broken(Data? values) { return values['key']; }",
        "function int broken(List<List<int>?> values) { return values[0][0]; }",
        "function int broken(Map<string,List<int>?> values) { return values['key'][0]; }",
        "function List<int>? missing() { return null; } "
        "function int broken() { return missing()[0]; }",
        "function void broken(List<int>? input) { let values = input; values[0] = 7; }",
        "function void broken(Map<string,int>? input) { let values = input; values['key'] += 1; }",
        "controller IndexedBox { List<int>? values; int broken() { return values[0]; } }",
        "function int broken(List<int>? input, bool reset) { let values = input; "
        "if (values == null) { return 0; } if (reset) { values = null; } return values[0]; }",
        "function int broken(List<int>? values) { const read = () => { "
        "if (values == null) { return 0; } return values[0]; }; return values[0]; }"
    };
    for (const auto source : sources) {
        auto options = deterministic_options();
        const auto emitted = Compiler{}.compile(source, "optional-index.gnr", options);
        options.validate_only = true;
        const auto checked = Compiler{}.compile(source, "optional-index.gnr", options);
        assert(!emitted.success() && !checked.success());
        assert(emitted.code.empty() && checked.code.empty());
        assert(!emitted.validated && !checked.validated);
        assert(emitted.diagnostics.size() == checked.diagnostics.size());
        assert_diagnostic_location(emitted, "optional-index.gnr");
        const auto found = std::find_if(
            emitted.diagnostics.begin(), emitted.diagnostics.end(),
            [](const auto& diagnostic) {
                return diagnostic.code == "GNR2201" &&
                    diagnostic.message == "Optional indexing requires a null guard";
            });
        assert(found != emitted.diagnostics.end());
        assert(found->span.valid);
        const auto indexed_source = source.substr(
            found->span.begin_offset,
            found->span.end_offset - found->span.begin_offset);
        assert(indexed_source.find('[') != std::string_view::npos);
        assert(indexed_source.find(']') != std::string_view::npos);
        for (std::size_t i = 0; i < emitted.diagnostics.size(); ++i) {
            assert(emitted.diagnostics[i].code == checked.diagnostics[i].code);
            assert(emitted.diagnostics[i].message == checked.diagnostics[i].message);
            assert(emitted.diagnostics[i].location.line == checked.diagnostics[i].location.line);
            assert(emitted.diagnostics[i].location.column == checked.diagnostics[i].location.column);
        }
    }
}

void guarded_indexing_preserves_immutability() {
    const std::vector<std::string_view> sources{
        "function void broken(List<int>? values) { "
        "if (values != null) { values[0] = 7; } }",
        "function void broken(Map<string,int>? input) { const values = input; "
        "if (values != null) { values['key'] += 1; } }",
        "model IndexedUser { table = 'indexed_users'; } "
        "function void broken(Collection<IndexedUser>? values, IndexedUser replacement) { "
        "if (values != null) { values[0] = replacement; } }",
        "function void broken(List<int>? input) { let values = input; "
        "if (values != null) { const write = () => { values[0] = 7; }; } }"
    };
    for (const auto source : sources) {
        const auto result = Compiler{}.compile(
            source, "immutable-index.gnr", deterministic_options());
        assert(!result.success());
        assert(result.code.empty() && !result.validated);
        assert(std::any_of(
            result.diagnostics.begin(), result.diagnostics.end(),
            [](const auto& diagnostic) { return diagnostic.code == "GNR2208"; }));
        assert(std::none_of(
            result.diagnostics.begin(), result.diagnostics.end(),
            [](const auto& diagnostic) {
                return diagnostic.message == "Optional indexing requires a null guard";
            }));
    }
}

void nullable_model_updates_require_guards_and_mutability() {
    struct InvalidCase {
        std::string_view source;
        std::string_view code;
    };
    const std::vector<InvalidCase> sources{
        {"model Counter { int? count; void update() { count += 1; } }", "GNR2201"},
        {"model Counter { double? count; void update() { count *= 2.0; } }", "GNR2201"},
        {"model Counter { string? label; void update() { label += '!'; } }", "GNR2201"},
        {"model Counter { int? count; void update() { count++; } }", "GNR2201"},
        {"model Counter { int? count; void update() { count--; } }", "GNR2201"},
        {"model Counter { int? count; void update() { if (count == null) { return; } "
         "count = null; count += 1; } }", "GNR2201"},
        {"model Counter { int? count; } function void update(Counter value) { "
         "value.count = 1; }", "GNR2208"},
        {"model Counter { int? count; void update() { if (count == null) { return; } "
         "const write = () => { count += 1; }; } }", "GNR2208"}
    };
    for (const auto& source : sources) {
        for (const bool validate_only : {false, true}) {
            auto options = deterministic_options();
            options.validate_only = validate_only;
            const auto result = Compiler{}.compile(source.source, "model-update.gnr", options);
            assert(!result.success());
            assert(result.code.empty() && !result.validated);
            assert_diagnostic_location(result, "model-update.gnr");
            assert(std::any_of(
                result.diagnostics.begin(), result.diagnostics.end(),
                [&](const auto& diagnostic) { return diagnostic.code == source.code; }));
        }
    }
}

void arrows_reject_implicit_instance_method_captures() {
    const std::vector<std::string_view> sources{
        "controller Counter { int helper() { return 1; } int run() { "
        "const read = () => helper(); return read(); } }",
        "controller Counter { int helper() { return 1; } int run() { "
        "const outer = () => { const inner = () => helper(); return inner(); }; return outer(); } }",
        "model Counter { int count; int helper() { return count; } int run() { "
        "const read = () => helper(); return read(); } }",
        "controller Counter { private int helper() { return 1; } int run() { "
        "const read = () => helper(); return read(); } }",
        "controller Counter { int count; int helper() { count++; return count; } int run() { "
        "const read = () => helper(); return read(); } }",
        "controller Counter { int helper() { return 1; } int run() { "
        "const read = () => { const callback = helper; return 0; }; return read(); } }",
        "controller Counter { int helper(int value) { return value; } int run() { "
        "return [1].map((item) => helper(item))[0]; } }",
        "controller Counter { int helper(int value = 2) { return value; } int run() { "
        "const read = () => helper(value: 3); return read(); } }",
        "controller Counter { int helper() { return 1; } int run() { "
        "const unused = () => helper(); return 0; } }"
    };
    for (const auto source : sources) {
        auto options = deterministic_options();
        const auto emitted = Compiler{}.compile(source, "arrow-method.gnr", options);
        options.validate_only = true;
        const auto checked = Compiler{}.compile(source, "arrow-method.gnr", options);
        assert(!emitted.success() && !checked.success());
        assert(emitted.code.empty() && checked.code.empty());
        assert(!emitted.validated && !checked.validated);
        assert(emitted.diagnostics.size() == checked.diagnostics.size());
        assert_diagnostic_location(emitted, "arrow-method.gnr");
        const auto found = std::find_if(
            emitted.diagnostics.begin(), emitted.diagnostics.end(),
            [](const auto& diagnostic) {
                return diagnostic.code == "GNR2208" && diagnostic.message ==
                    "Arrows cannot implicitly capture instance methods; use a mutable local receiver";
            });
        assert(found != emitted.diagnostics.end());
        assert(found->span.valid);
        assert(source.substr(found->span.begin_offset,
            found->span.end_offset - found->span.begin_offset) == "helper");
        for (std::size_t i = 0; i < emitted.diagnostics.size(); ++i) {
            assert(emitted.diagnostics[i].code == checked.diagnostics[i].code);
            assert(emitted.diagnostics[i].message == checked.diagnostics[i].message);
            assert(emitted.diagnostics[i].span.begin_offset == checked.diagnostics[i].span.begin_offset);
            assert(emitted.diagnostics[i].span.end_offset == checked.diagnostics[i].span.end_offset);
        }
    }
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
    structured_model_metadata_is_canonical();
    model_naming_and_cast_contracts_are_canonical();
    framework_contracts_fail_before_codegen();
    optional_flow_rejects_stale_nonnull_facts();
    optional_indexing_requires_a_nonnull_receiver();
    guarded_indexing_preserves_immutability();
    nullable_model_updates_require_guards_and_mutability();
    arrows_reject_implicit_instance_method_captures();
}
