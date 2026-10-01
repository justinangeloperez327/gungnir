#pragma once
#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <gungnir/language/lexer.hpp>

namespace gungnir::language {
class Formatter {
public:
    [[nodiscard]] std::string format(std::string_view source) const {
        std::vector<Diagnostic> diagnostics;
        const auto scanned = Lexer{source}.tokenize(&diagnostics,"<format>",true);
        if (!diagnostics.empty()) throw std::invalid_argument(diagnostics.front().message);
        const auto significant = [](const std::vector<Token>& input) {
            static const std::unordered_set<std::string> pairs{"::","=>","??","?.","==","!=","<=",">=","&&","||","++","--","+=","-=","*=","/=","->","<<",">>"};
            std::vector<Token> result;
            for (std::size_t i=0;i<input.size();++i) {
                if (input[i].kind == TokenKind::whitespace || input[i].kind == TokenKind::end) continue;
                auto token = input[i];
                if (i+1 < input.size() && token.kind == TokenKind::symbol && input[i+1].kind == TokenKind::symbol && input[i+1].offset == token.offset+token.lexeme.size() && pairs.contains(token.lexeme+input[i+1].lexeme)) token.lexeme += input[++i].lexeme;
                result.push_back(std::move(token));
            }
            return result;
        };
        const auto tokens = significant(scanned);
        for (const auto& token : tokens) if ((token.kind == TokenKind::symbol && (token.lexeme == "#" || token.lexeme == "\\")) || (token.kind == TokenKind::comment && token.lexeme.starts_with("#")))
            throw std::invalid_argument("Native preprocessor directives and line continuations require a C++ formatter");
        std::vector<char> delimiters;
        std::string out, previous;
        std::size_t indent = 0;
        auto trim = [&] { while (!out.empty() && out.back() == ' ') out.pop_back(); };
        auto newline = [&] { trim(); if (!out.empty() && out.back() != '\n') out += '\n'; };
        auto space = [&] { if (!out.empty() && out.back() != '\n' && out.back() != ' ') out += ' '; };
        auto put = [&](std::string_view text) { if (out.empty() || out.back() == '\n') out.append(indent*4,' '); out += text; };
        for (std::size_t i=0;i<tokens.size();++i) {
            const auto& token = tokens[i]; const auto& value = token.lexeme;
            const auto next = i+1 < tokens.size() ? tokens[i+1].lexeme : std::string{};
            if (token.kind == TokenKind::comment) {
                // Preserve comment bytes and force the lexical end of // comments.
                if (i && token.line > tokens[i-1].line) newline(); else space();
                put(value);
                if (value.starts_with("//") || value.find('\n') != std::string::npos) newline(); else space();
                continue;
            }
            if (value == "{") { space(); put(value); delimiters.push_back('{'); ++indent; newline(); }
            else if (value == "}") {
                if (delimiters.empty() || delimiters.back() != '{') throw std::invalid_argument("Unbalanced formatting delimiters");
                delimiters.pop_back(); --indent; newline(); put(value);
                if (next != ";" && next != "," && next != ")" && next != "]" && next != "." && next != "::" && next != "else") newline();
            } else if (value == "(" || value == "[") {
                if (value == "(" && (previous == "if" || previous == "while" || previous == "for" || previous == "switch")) space();
                else if (value == "[" && (previous == "return" || previous == "=")) space();
                trim(); if (value == "(" && (previous == "if" || previous == "while" || previous == "for")) space();
                put(value); delimiters.push_back(value.front());
            } else if (value == ")" || value == "]") {
                const char expected = value == ")" ? '(' : '[';
                if (delimiters.empty() || delimiters.back() != expected) throw std::invalid_argument("Unbalanced formatting delimiters");
                delimiters.pop_back(); trim(); put(value);
            } else if (value == ";") {
                trim(); put(value);
                if (!delimiters.empty() && delimiters.back() == '(') space(); else newline();
            } else if (value == ",") { trim(); put(value); space(); }
            else if (value == "." || value == "::" || value == "?.") { trim(); put(value); }
            else if (value == ":") { trim(); put(value); space(); }
            else {
                if (!out.empty() && previous != "(" && previous != "[" && previous != "." && previous != "::" && previous != "?.") space();
                put(value);
            }
            previous = value;
        }
        if (!delimiters.empty()) throw std::invalid_argument("Unbalanced formatting delimiters");
        newline();
        // Whitespace must never merge operators, change literals, or swallow a
        // token into a comment. This invariant also covers compatibility syntax.
        const auto formatted = significant(Lexer{out}.tokenize(nullptr,"<format>",true));
        if (tokens.size() != formatted.size()) throw std::invalid_argument("Formatting would change the token stream");
        for (std::size_t i=0;i<tokens.size();++i) if (tokens[i].kind != formatted[i].kind || tokens[i].lexeme != formatted[i].lexeme)
            throw std::invalid_argument("Formatting would change the token stream");
        return out;
    }
};
}
