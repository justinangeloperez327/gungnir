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
           "[--no-line-directives] [--project] [--dump-validated-ast] "
           "[--dump-cpp-ir] [--format] [--compat] [--strict]\n";
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
    bool compatibility = false, compatibility_requested = false;
    bool project = false, dump_validated = false, dump_ir = false;

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

        if (argument == "--strict") { compatibility = false; continue; }
        if (argument == "--compat") { compatibility = true; compatibility_requested = true; continue; }
        if (argument == "--project") { project = true; continue; }
        if (argument == "--dump-validated-ast") { dump_validated = true; continue; }
        if (argument == "--dump-cpp-ir") { dump_ir = true; continue; }
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

    if (dump_validated && dump_ir) {
        std::cerr << "gungnirc: error: choose one compiler dump mode\n";
        return 2;
    }

    if (compatibility_requested && (project || dump_validated || dump_ir)) {
        std::cerr << "gungnirc: error: --compat cannot be combined with project or structured compiler dumps\n";
        return 2;
    }

    try {
        if (format_only) {
            if (project || dump_validated || dump_ir) throw std::invalid_argument("--format requires a single source file and cannot dump compiler IR");
            const auto source = read_file(input_path);
            if (!compatibility) {
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

        if (!compatibility) {
            gungnir::language::Compiler compiler;
            gungnir::language::CompilerOptions options;
            options.emit_line_directives = emit_line_directives;
            options.validate_only = check_only || dump_validated || dump_ir;
            const auto result = project ? compiler.compile_project(input_path,options) : compiler.compile(read_file(input_path),input_path.generic_string(),options);
            for (const auto& diagnostic : result.diagnostics) {
                std::cerr
                    << gungnir::language::DiagnosticRenderer::render(diagnostic)
                    << '\n';
            }
            if (!result.success()) return 1;
            if (!check_only || dump_validated || dump_ir) {
                std::string content;
                if (dump_validated) {
                    content = gungnir::language::dump_validated(*result.validated);
                } else if (dump_ir) {
                    content = gungnir::language::dump_cpp_ir(
                        gungnir::language::CppIrLowerer{}.lower(
                            *result.validated,
                            emit_line_directives
                        )
                    );
                } else {
                    content = result.code;
                }

                if (output_path.empty()) std::cout << content;
                else write_file(output_path,content);
            }
            return 0;
        }
        const auto source = read_file(input_path);


        if (project || dump_validated || dump_ir) throw std::invalid_argument("--compat does not support structured compiler modes");
        gungnir::language::CompatibilityTranspiler transpiler;
        const auto result = transpiler.transpile(
            source,
            input_path.generic_string(),
            gungnir::language::TranspileOptions{
                .emit_line_directives = emit_line_directives
            }
        );

        for (const auto& diagnostic : result.diagnostics) {
            std::cerr
                << gungnir::language::DiagnosticRenderer::render(
                    diagnostic,
                    source
                )
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

