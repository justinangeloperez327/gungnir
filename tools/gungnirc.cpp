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
           "[--no-line-directives]\n";
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
        const auto source = read_file(input_path);

        if (format_only) {
            gungnir::language::Formatter formatter;
            const auto formatted = formatter.format(source);
            if (output_path.empty()) std::cout << formatted;
            else write_file(output_path, formatted);
            return 0;
        }

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
