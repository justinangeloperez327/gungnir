#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <gungnir/language/language.hpp>
#include <gungnir/language/diagnostic_renderer.hpp>

namespace {

void usage() {
    std::cerr
        << "Usage: gungnirc <input.gnr> [-o output.cpp] [--check] "
           "[--no-line-directives] [--strict] [--project] [--dump-validated-ast] [--format]\n";
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error("Unable to open input file: " + path.string());
    }

    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };
}

void write_file(
    const std::filesystem::path& path,
    const std::string& content
) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output) {
        throw std::runtime_error("Unable to open output file: " + path.string());
    }

    output << content;
}

const char* level_name(gungnir::language::DiagnosticLevel level) {
    return level == gungnir::language::DiagnosticLevel::error
        ? "error"
        : "warning";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 2;
    }

    std::filesystem::path input_path;
    std::filesystem::path output_path;
    bool check_only = false;
    bool emit_line_directives = true;
    bool format_only = false;
    bool strict = false, project = false, dump = false;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];

        if (argument == "-o") {
            if (index + 1 >= argc) {
                usage();
                return 2;
            }

            output_path = argv[++index];
            continue;
        }

        if (argument == "--strict") { strict = true; continue; }
        if (argument == "--project") { strict = true; project = true; continue; }
        if (argument == "--dump-validated-ast") { strict = true; dump = true; continue; }
        if (argument == "--check") {
            check_only = true;
            continue;
        }

        if (argument == "--format") {
            format_only = true;
            continue;
        }

        if (argument == "--no-line-directives") {
            emit_line_directives = false;
            continue;
        }

        if (!argument.empty() && argument.front() == '-') {
            usage();
            return 2;
        }

        if (!input_path.empty()) {
            usage();
            return 2;
        }

        input_path = argument;
    }

    if (input_path.empty()) {
        usage();
        return 2;
    }

    try {
        if (format_only) {
            if (project || dump) throw std::invalid_argument("--format requires a single source file and cannot dump an AST");
            const auto source = read_file(input_path);
            if (strict) {
                auto syntax = gungnir::language::SyntaxParser{}.parse(source,input_path.generic_string());
                if (!syntax.diagnostics.empty()) throw std::invalid_argument(syntax.diagnostics.front().message);
            }
            gungnir::language::Formatter formatter;
            const auto formatted = formatter.format(source);
            if (check_only) return formatted == source ? 0 : 1;
            if (output_path.empty()) std::cout << formatted;
            else write_file(output_path, formatted);
            return 0;
        }

        if (strict) {
            gungnir::language::Compiler compiler;
            gungnir::language::CompilerOptions options; options.emit_line_directives = emit_line_directives;
            const auto result = project ? compiler.compile_project(input_path,options) : compiler.compile(read_file(input_path),input_path.generic_string(),options);
            for (const auto& diagnostic : result.diagnostics) std::cerr << diagnostic.location.file << ':' << diagnostic.location.line << ':' << diagnostic.location.column << ": " << diagnostic.code << ": " << diagnostic.message << '\n';
            if (!result.success()) return 1;
            if (!check_only || dump) { const auto content = dump ? gungnir::language::dump_validated(*result.validated) : result.code; if (output_path.empty()) std::cout << content; else write_file(output_path,content); }
            return 0;
        }
        const auto source = read_file(input_path);


        gungnir::language::Transpiler transpiler;
        const auto result = transpiler.transpile(
            source,
            input_path.generic_string(),
            gungnir::language::TranspileOptions{
                .emit_line_directives = emit_line_directives
            }
        );

        for (const auto& diagnostic : result.diagnostics) {
            std::cerr
                << gungnir::language::DiagnosticRenderer::render(diagnostic)
                << '\n';
        }

        if (!result.success()) {
            return 1;
        }

        if (check_only) {
            return 0;
        }

        if (output_path.empty()) {
            std::cout << result.code;
        } else {
            write_file(output_path, result.code);
        }
    } catch (const std::exception& error) {
        std::cerr << "gungnirc: error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}

