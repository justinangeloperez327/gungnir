#include <gungnir/language/compiler.hpp>
#include <gungnir/language/lexer.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using namespace gungnir::language;

namespace {

void assert_diagnostics_bounded(
    const std::vector<Diagnostic>& diagnostics,
    std::size_t source_size
) {
    for (const auto& diagnostic : diagnostics) {
        assert(diagnostic.location.line >= 1);
        assert(diagnostic.location.column >= 1);
        if (diagnostic.span.valid) {
            assert(diagnostic.span.begin_offset <= source_size);
            assert(diagnostic.span.end_offset <= source_size);
            assert(
                diagnostic.span.end_offset >=
                diagnostic.span.begin_offset
            );
        }
    }
}

void exercise(std::string_view source, std::size_t id) {
    const auto file = "robustness-" + std::to_string(id) + ".gnr";

    std::vector<Diagnostic> lexer_diagnostics;
    const auto tokens = Lexer{source}.tokenize(
        &lexer_diagnostics,
        file,
        true
    );
    assert(!tokens.empty());
    assert_diagnostics_bounded(
        lexer_diagnostics,
        source.size()
    );

    const auto parsed = SyntaxParser{}.parse(
        source,
        file
    );
    assert_diagnostics_bounded(
        parsed.diagnostics,
        source.size()
    );

    CompilerOptions options;
    options.emit_line_directives = false;
    options.validate_only = true;
    const auto compiled = Compiler{}.compile(
        source,
        file,
        options
    );
    assert_diagnostics_bounded(
        compiled.diagnostics,
        source.size()
    );

    if (compiled.success()) {
        assert(compiled.validated.has_value());
        assert(compiled.code.empty());
    } else {
        assert(!compiled.validated.has_value());
        assert(compiled.code.empty());
    }
}

std::vector<std::string> corpus() {
    std::vector<std::string> cases{
        "",
        "function",
        "function int missing( {",
        "function int f() { return \"unterminated; }",
        "function int f() { return R\"tag(unterminated; }",
        "/* never closed",
        "function int f() { return 1.2.3; }",
        "function int f() { return missing; }",
        "function int f() { if (true) { return 1; } }",
        "function int f() { while (true) { continue; } }",
        "controller Home { Response index() { return text(\"ok\"); } }",
        "model User { string name; }",
        "function string? f(string? value) { return value; }",
        "function int f() { const value = [1, 2, 3]; return value[0]; }",
        "function int f() { return true ? 1 : 2; }",
        "function int f() { return (((((((1))))))); }"
    };

    cases.emplace_back(1024, '{');
    cases.emplace_back(1024, '}');
    cases.emplace_back(4096, '/');
    cases.emplace_back(4096, '0');

    std::string nul{
        "function int f() { return 1; }",
        30
    };
    nul.insert(nul.begin() + 8, '\0');
    cases.push_back(std::move(nul));

    std::string invalid_utf8{
        "function int f() { return \""
    };
    invalid_utf8.push_back(static_cast<char>(0xff));
    invalid_utf8.push_back(static_cast<char>(0xfe));
    invalid_utf8 += "\"; }";
    cases.push_back(std::move(invalid_utf8));

    std::string deep_expression{
        "function int f() { return "
    };
    deep_expression.append(320, '(');
    deep_expression += "1";
    deep_expression.append(320, ')');
    deep_expression += "; }";
    cases.push_back(std::move(deep_expression));

    std::string deep_blocks{
        "function void f() {"
    };
    deep_blocks.append(320, '{');
    deep_blocks += "return;";
    deep_blocks.append(320, '}');
    deep_blocks += "}";
    cases.push_back(std::move(deep_blocks));

    std::string deep_type{"function "};
    for (int i = 0; i < 300; ++i) {
        deep_type += "List<";
    }
    deep_type += "int";
    for (int i = 0; i < 300; ++i) {
        deep_type += ">";
    }
    deep_type += " f() { return []; }";
    cases.push_back(std::move(deep_type));

    return cases;
}

void deterministic_mutation_sweep(
    const std::vector<std::string>& seeds
) {
    static constexpr char replacements[]{
        '\0', '{', '}', '(', ')', '[', ']', '"', '\'', '?', ';', '/'
    };

    std::size_t id = 1000;
    for (const auto& seed : seeds) {
        exercise(seed, id++);

        if (!seed.empty()) {
            for (
                std::size_t cut = 0;
                cut < seed.size();
                cut += std::max<std::size_t>(
                    1,
                    seed.size() / 8
                )
            ) {
                exercise(
                    std::string_view{seed}.substr(0, cut),
                    id++
                );
            }
        }

        const auto stride = std::max<std::size_t>(
            1,
            seed.size() / 12
        );
        for (
            std::size_t position = 0;
            position < seed.size();
            position += stride
        ) {
            for (char replacement : replacements) {
                auto mutated = seed;
                mutated[position] = replacement;
                exercise(mutated, id++);
            }
        }
    }
}

} // namespace

int main() {
    const auto seeds = corpus();
    deterministic_mutation_sweep(seeds);
}
