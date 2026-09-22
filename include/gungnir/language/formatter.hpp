#pragma once
#include <string>
#include <string_view>
#include <gungnir/language/lexer.hpp>

namespace gungnir::language {

class Formatter {
public:
    [[nodiscard]] std::string format(std::string_view source) const {
        const auto tokens = Lexer{source}.tokenize();
        std::string out;
        std::size_t indent = 0;
        bool line_start = true;

        const auto write_indent = [&]() mutable {
            if (line_start) {
                out.append(indent * 4, ' ');
                line_start = false;
            }
        };

        for (const auto& token : tokens) {
            if (token.kind == TokenKind::end) break;
            if (token.kind == TokenKind::string_literal || token.kind == TokenKind::character_literal || token.kind == TokenKind::comment) {
                write_indent(); out += token.lexeme; continue;
            }
            if (token.kind == TokenKind::whitespace) {
                if (token.lexeme.find('\n') != std::string::npos) {
                    while (!out.empty() && out.back() == ' ') out.pop_back();
                    if (out.empty() || out.back() != '\n') out += '\n';
                    line_start = true;
                } else if (!line_start && !out.empty() && out.back() != ' ') out += ' ';
                continue;
            }
            if (token.lexeme == "}") {
                if (!line_start) { out += '\n'; line_start = true; }
                if (indent > 0) --indent;
                write_indent(); out += '}';
                continue;
            }
            write_indent();
            out += token.lexeme;
            if (token.lexeme == "{") {
                ++indent; out += '\n'; line_start = true;
            } else if (token.lexeme == ";") {
                out += '\n'; line_start = true;
            }
        }
        while (!out.empty() && (out.back() == ' ' || out.back() == '\n')) out.pop_back();
        out += '\n';
        return out;
    }
};

} // namespace gungnir::language
