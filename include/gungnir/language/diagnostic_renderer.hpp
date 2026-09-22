#pragma once
#include <sstream>
#include <string>
#include <string_view>
#include <gungnir/language/diagnostic.hpp>

namespace gungnir::language {

class DiagnosticRenderer {
public:
    [[nodiscard]] static std::string render(const Diagnostic& diagnostic, std::string_view source = {}) const {
        std::ostringstream out;
        out << diagnostic.location.file << ':' << diagnostic.location.line << ':' << diagnostic.location.column
            << ": " << (diagnostic.level == DiagnosticLevel::error ? "error" : "warning");
        if (!diagnostic.code.empty()) out << '[' << diagnostic.code << ']';
        out << ": " << diagnostic.message << '\n';
        if (!source.empty()) {
            std::size_t line = 1, start = 0;
            while (line < diagnostic.location.line && start < source.size()) {
                const auto next = source.find('\n', start);
                if (next == std::string_view::npos) break;
                start = next + 1; ++line;
            }
            const auto end = source.find('\n', start);
            const auto text = source.substr(start, end == std::string_view::npos ? source.size() - start : end - start);
            out << "  " << text << '\n' << "  " << std::string(diagnostic.location.column > 0 ? diagnostic.location.column - 1 : 0, ' ') << "^\n";
        }
        if (!diagnostic.hint.empty()) out << "hint: " << diagnostic.hint << '\n';
        return out.str();
    }
};

} // namespace gungnir::language
