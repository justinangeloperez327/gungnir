#include <gungnir/language/compiler.hpp>
#include <gungnir/language/spec.hpp>

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace gungnir::language;

namespace {

bool has_code(
    const CompilationResult& result,
    std::string_view code
) {
    return std::any_of(
        result.diagnostics.begin(),
        result.diagnostics.end(),
        [&](const auto& diagnostic) {
            return diagnostic.code == code;
        }
    );
}

CompilationResult check(
    std::string_view source,
    std::string file
) {
    CompilerOptions options;
    options.emit_line_directives = false;
    options.validate_only = true;

    return Compiler{}.compile(
        source,
        std::move(file),
        options
    );
}

void release_metadata_is_consistent() {
    static_assert(language_name == "Gungnir");
    static_assert(language_version == "1.0");
    static_assert(compiler_contract_version == "1.0");
    static_assert(diagnostic_contract_version == "1.0");
    static_assert(structured_profile_feature_frozen);
    static_assert(
        compiler_compatibility ==
        Compatibility::stable
    );

    const CompilerOptions defaults;
    assert(defaults.emit_line_directives);
    assert(!defaults.validate_only);
    assert(!defaults.verify_ir_only);
}

void development_acceptance_contract() {
    const std::vector<std::string_view> accepted{
        "function int answer() { return 42; }",
        "function string require(string? value) { "
            "if (value == null) { return \"fallback\"; } "
            "return value; }",
        "async function int load() { return 1; }",
        "function int choose(bool flag) { "
            "if (flag) { return 1; } else { return 2; } }"
    };

    for (std::size_t i = 0; i < accepted.size(); ++i) {
        const auto result = check(
            accepted[i],
            "stability-pass-" + std::to_string(i) + ".gnr"
        );

        assert(result.success());
        assert(result.validated.has_value());
        assert(result.code.empty());
        assert(result.diagnostics.empty());
    }
}

void development_diagnostic_codes() {
    struct Case {
        std::string_view source;
        std::string_view code;
    };

    const std::vector<Case> rejected{
        {
            "function int broken() { return absent; }",
            "GNR2202"
        },
        {
            "function int missing(bool flag) { "
            "if (flag) { return 1; } }",
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

    for (std::size_t i = 0; i < rejected.size(); ++i) {
        const auto file =
            "stability-fail-" + std::to_string(i) + ".gnr";
        const auto result = check(rejected[i].source, file);

        assert(!result.success());
        assert(!result.validated.has_value());
        assert(result.code.empty());
        assert(has_code(result, rejected[i].code));

        for (const auto& diagnostic : result.diagnostics) {
            assert(!diagnostic.code.empty());
            assert(diagnostic.location.file == file);
            assert(diagnostic.location.line >= 1);
            assert(diagnostic.location.column >= 1);
        }
    }
}

void development_check_and_compile_semantics_match() {
    constexpr std::string_view source =
        "function string require(string? value) { "
        "if (value == null) { throw \"missing\"; } "
        "return value; }";

    auto checked_options = CompilerOptions{};
    checked_options.emit_line_directives = false;
    checked_options.validate_only = true;

    auto compile_options = checked_options;
    compile_options.validate_only = false;
    auto verify_options = compile_options;
    verify_options.verify_ir_only = true;

    const auto checked = Compiler{}.compile(
        source,
        "stability-parity.gnr",
        checked_options
    );
    const auto compiled = Compiler{}.compile(
        source,
        "stability-parity.gnr",
        compile_options
    );
    const auto verified = Compiler{}.compile(
        source,
        "stability-parity.gnr",
        verify_options
    );

    assert(verified.success());
    assert(verified.validated.has_value());
    assert(verified.code.empty());
    assert(verified.diagnostics.empty());
    assert(checked.success());
    assert(compiled.success());
    assert(checked.validated.has_value());
    assert(compiled.validated.has_value());
    assert(checked.code.empty());
    assert(!compiled.code.empty());
    assert(
        compiled.code.find(
            "compiler_contract_version == \"1.0\""
        ) != std::string::npos
    );
    assert(checked.diagnostics.empty());
    assert(compiled.diagnostics.empty());

    assert(
        dump_validated(*checked.validated) ==
        dump_validated(*compiled.validated)
    );
    assert(
        dump_validated(*verified.validated) ==
        dump_validated(*compiled.validated)
    );

    const auto invalid = Compiler{}.compile(
        "function int broken() { return unknown; }",
        "stability-ir-invalid.gnr",
        verify_options
    );
    assert(!invalid.success());
    assert(invalid.code.empty());
    assert(has_code(invalid, "GNR2202"));

    bool rejected_conflicting_options = false;
    try {
        auto conflicting = checked_options;
        conflicting.verify_ir_only = true;
        (void)Compiler{}.compile(source, "conflicting-modes.gnr", conflicting);
    } catch (const std::invalid_argument&) {
        rejected_conflicting_options = true;
    }
    assert(rejected_conflicting_options);
}

} // namespace

int main() {
    release_metadata_is_consistent();
    development_acceptance_contract();
    development_diagnostic_codes();
    development_check_and_compile_semantics_match();
}
