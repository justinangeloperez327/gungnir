#pragma once
#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <gungnir/language/diagnostic.hpp>

namespace gungnir::language {

class DiagnosticRenderer {
public:
    [[nodiscard]] static std::string render(
        const Diagnostic& diagnostic,
        std::string_view source = {}
    ) {
        std::ostringstream out;
        out << diagnostic.location.file << ':'
            << diagnostic.location.line << ':'
            << diagnostic.location.column << ": "
            << (diagnostic.level == DiagnosticLevel::error
                    ? "error"
                    : "warning");
        if (!diagnostic.code.empty()) {
            out << '[' << diagnostic.code << ']';
        }
        out << ": " << diagnostic.message << '\n';

        std::string_view line_text = diagnostic.source_line;
        if (line_text.empty() && !source.empty()) {
            line_text = source_line(source, diagnostic.location.line);
        }

        if (!line_text.empty()) {
            const auto line_number =
                std::to_string(diagnostic.location.line);
            out << line_number << " | " << line_text << '\n';
            out << std::string(line_number.size(), ' ') << " | ";

            const auto column =
                diagnostic.location.column > 0
                    ? diagnostic.location.column
                    : 1;
            out << std::string(column - 1, ' ');

            std::size_t width = 1;
            if (
                diagnostic.span.valid &&
                diagnostic.span.end_line == diagnostic.location.line &&
                diagnostic.span.end_column > column
            ) {
                width =
                    diagnostic.span.end_column - column;
            }

            out << '^';
            if (width > 1) {
                out << std::string(width - 1, '~');
            }
            out << '\n';
        }

        if (!diagnostic.hint.empty()) {
            out << "help: " << diagnostic.hint << '\n';
        }
        return out.str();
    }

private:
    [[nodiscard]] static std::string_view source_line(
        std::string_view source,
        std::size_t target_line
    ) {
        if (target_line == 0) {
            return {};
        }

        std::size_t line = 1;
        std::size_t start = 0;
        while (line < target_line && start < source.size()) {
            const auto next = source.find('\n', start);
            if (next == std::string_view::npos) {
                return {};
            }
            start = next + 1;
            ++line;
        }

        if (line != target_line || start > source.size()) {
            return {};
        }

        const auto end = source.find('\n', start);
        auto text = source.substr(
            start,
            end == std::string_view::npos
                ? source.size() - start
                : end - start
        );
        if (!text.empty() && text.back() == '\r') {
            text.remove_suffix(1);
        }
        return text;
    }
};

} // namespace gungnir::language
