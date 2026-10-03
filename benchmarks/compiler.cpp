#include "benchmark.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include <gungnir/language/compiler.hpp>

namespace {

std::string make_program() {
    std::string source;
    source.reserve(32000);

    source +=
        "model User {\n"
        "  table = 'users';\n"
        "  fillable = ['name', 'email'];\n"
        "  timestamps = true;\n"
        "  softDeletes = true;\n"
        "}\n";

    for (
        std::size_t index = 0;
        index < 128;
        ++index
    ) {
        source +=
            "function int compute_" +
            std::to_string(index) +
            "(int value) { "
            "const doubled = value * 2; "
            "if (doubled > 10) { "
            "return doubled + " +
            std::to_string(index) +
            "; } "
            "return doubled; }\n";
    }

    source +=
        "controller UsersController {\n"
        "  index(Request request) {\n"
        "    return json({ ok: true, path: request.path() });\n"
        "  }\n"
        "}\n";

    return source;
}

std::uint64_t parser_case(
    const std::string& source,
    std::size_t iteration
) {
    const auto result =
        gungnir::language::SyntaxParser{}
            .parse(
                source,
                "benchmark.gnr",
                "app"
            );

    if (!result.diagnostics.empty()) {
        throw std::runtime_error(
            "Compiler parser benchmark produced diagnostics"
        );
    }

    return
        static_cast<std::uint64_t>(
            result.project.declarations.size() +
            result.project.expressions.size() +
            result.project.statements.size() +
            iteration
        );
}

std::uint64_t check_case(
    const std::string& source,
    std::size_t iteration
) {
    gungnir::language::CompilerOptions
        options;

    options.emit_line_directives = false;
    options.validate_only = true;

    const auto result =
        gungnir::language::Compiler{}
            .compile(
                source,
                "benchmark.gnr",
                options
            );

    if (!result.success()) {
        throw std::runtime_error(
            "Compiler check benchmark failed"
        );
    }

    return
        static_cast<std::uint64_t>(
            result.validated
                ? result.validated
                    ->symbols()
                    .size()
                : 0
        ) +
        static_cast<std::uint64_t>(
            iteration
        );
}

std::uint64_t compile_case(
    const std::string& source,
    std::size_t iteration
) {
    gungnir::language::CompilerOptions
        options;

    options.emit_line_directives = false;

    const auto result =
        gungnir::language::Compiler{}
            .compile(
                source,
                "benchmark.gnr",
                options
            );

    if (!result.success()) {
        throw std::runtime_error(
            "Compiler full benchmark failed"
        );
    }

    return
        static_cast<std::uint64_t>(
            result.code.size()
        ) +
        static_cast<std::uint64_t>(
            iteration
        );
}

} // namespace

int main(
    int argc,
    char** argv
) {
    auto config =
        gungnir::benchmark::
            parse_config(
                argc,
                argv
            );

    if (
        config.iterations == 1000 &&
        config.samples == 9
    ) {
        // Full compilation is intentionally heavier than
        // the runtime microbenchmarks.
        config.iterations = 8;
        config.samples = 7;
        config.warmup_iterations = 2;
    }

    const auto source =
        make_program();

    gungnir::benchmark::Suite suite{
        "compiler",
        config
    };

    suite.run(
        "parse_large_program",
        [&](std::size_t iteration) {
            return parser_case(
                source,
                iteration
            );
        }
    );

    suite.run(
        "authoritative_check_large_program",
        [&](std::size_t iteration) {
            return check_case(
                source,
                iteration
            );
        }
    );

    suite.run(
        "full_compile_large_program",
        [&](std::size_t iteration) {
            return compile_case(
                source,
                iteration
            );
        }
    );

    return suite.finish();
}
