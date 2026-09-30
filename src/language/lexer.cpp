#include <gungnir/language/lexer.hpp>

#include <cctype>
#include <regex>
#include <string>
#include <string_view>

namespace gungnir::language {

namespace {

bool keyword(std::string_view value) {
    return
        value == "async" ||
        value == "await" ||
        value == "bool" ||
        value == "boolean" ||
        value == "connection" ||
        value == "controller" ||
        value == "double" ||
        value == "event" ||
        value == "float" ||
        value == "inject" ||
        value == "int" ||
        value == "int64" ||
        value == "integer" ||
        value == "listener" ||
        value == "mail" ||
        value == "middleware" ||
        value == "migration" ||
        value == "model" ||
        value == "notification" ||
        value == "policy" ||
        value == "softDeletes" ||
        value == "string" ||
        value == "table" ||
        value == "timestamps" ||
        value == "uint64" ||
        value == "validation";
}

} // namespace

Lexer::Lexer(std::string_view source) noexcept : source_(source) {}

std::vector<Token> Lexer::tokenize(
    std::vector<Diagnostic>* diagnostics, std::string_view source_name, bool single_quoted_strings
) const {
    std::vector<Token> tokens;

    std::size_t index = 0;
    std::size_t line = 1;
    std::size_t column = 1;

    const auto report = [&](std::size_t line, std::size_t column,
                            std::string message, std::string code) {
        if (diagnostics) diagnostics->push_back(Diagnostic{
            DiagnosticLevel::error, {std::string{source_name}, line, column},
            std::move(message), std::move(code), {}});
    };
    auto advance = [&]() {
        const char current = source_[index++];
        if (current == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    };

    const auto emit = [&](
        TokenKind kind,
        std::size_t start,
        std::size_t start_line,
        std::size_t start_column
    ) {
        tokens.push_back(Token{
            kind,
            std::string{source_.substr(start, index - start)},
            start,
            start_line,
            start_column
        });
    };

    while (index < source_.size()) {
        const auto start = index;
        const auto start_line = line;
        const auto start_column = column;
        const char current = source_[index];

        if (current == '#') {
            const auto line_start = source_.rfind('\n', start);
            const auto prefix = source_.substr(
                line_start == std::string_view::npos ? 0 : line_start + 1,
                start - (line_start == std::string_view::npos ? 0 : line_start + 1)
            );
            if (prefix.find_first_not_of(" \t\r") == std::string_view::npos) {
                while (index < source_.size()) {
                    if (source_[index] == '\n') {
                        const bool continued = index > start &&
                            source_[index - 1] == '\\';
                        if (!continued) break;
                    }
                    advance();
                }
                emit(TokenKind::comment, start, start_line, start_column);
                continue;
            }
        }

        if (std::isspace(static_cast<unsigned char>(current)) != 0) {
            while (
                index < source_.size() &&
                std::isspace(static_cast<unsigned char>(source_[index])) != 0
            ) {
                advance();
            }

            emit(TokenKind::whitespace, start, start_line, start_column);
            continue;
        }

        if (
            current == '/' &&
            index + 1 < source_.size() &&
            source_[index + 1] == '/'
        ) {
            advance();
            advance();

            while (index < source_.size() && source_[index] != '\n') {
                advance();
            }

            emit(TokenKind::comment, start, start_line, start_column);
            continue;
        }

        if (
            current == '/' &&
            index + 1 < source_.size() &&
            source_[index + 1] == '*'
        ) {
            advance();
            advance();
            bool closed = false;

            while (index < source_.size()) {
                if (
                    source_[index] == '*' &&
                    index + 1 < source_.size() &&
                    source_[index + 1] == '/'
                ) {
                    advance();
                    advance();
                    closed = true;
                    break;
                }

                advance();
            }

            if (!closed) report(start_line, start_column,
                                "Unterminated block comment", "GNR0901");
            emit(TokenKind::comment, start, start_line, start_column);
            continue;
        }

        if (
            current == 'R' &&
            index + 1 < source_.size() &&
            source_[index + 1] == '"'
        ) {
            advance();
            advance();

            bool raw_closed = false;
            const auto delimiter_start = index;
            while (index < source_.size() && source_[index] != '(') {
                advance();
            }

            if (index < source_.size()) {
                const std::string delimiter{
                    source_.substr(delimiter_start, index - delimiter_start)
                };
                if (delimiter.size() > 16 || delimiter.find_first_of(" \\)\t\r\n") != std::string::npos)
                    report(start_line, start_column, "Invalid raw string delimiter", "GNR0902");
                advance();

                const std::string closing = ")" + delimiter + "\"";
                const auto closing_at = source_.find(closing, index);

                if (closing_at == std::string_view::npos) {
                    while (index < source_.size()) {
                        advance();
                    }
                } else {
                    raw_closed = true;
                    const auto end = closing_at + closing.size();
                    while (index < end) {
                        advance();
                    }
                }
            }

            if (!raw_closed) report(start_line, start_column,
                                    "Unterminated raw string literal", "GNR0902");
            emit(TokenKind::string_literal, start, start_line, start_column);
            continue;
        }

        if (current == '"' || current == '\'') {
            const char quote = current;
            bool escaped = false;
            bool closed = false;
            advance();

            while (index < source_.size()) {
                const char value = source_[index];
                advance();

                if (escaped) {
                    escaped = false;
                    continue;
                }

                if (value == '\\') {
                    escaped = true;
                    continue;
                }

                if (value == '\n' || value == '\r') {
                    report(start_line, start_column, "Newline in quoted literal", "GNR0902");
                    break;
                }
                if (value == quote) {
                    closed = true;
                    break;
                }
            }

            if (!closed) report(start_line, start_column,
                                "Unterminated quoted literal", "GNR0902");
            if (closed && quote == '\'' && !single_quoted_strings) {
                static const std::regex character{R"('([^'\\\r\n]|\\([abfnrtv\\'"?]|[0-7]{1,3}|x[0-9a-fA-F]+|u[0-9a-fA-F]{4}|U[0-9a-fA-F]{8}))')"};
                const std::string literal{source_.substr(start, index - start)};
                if (!std::regex_match(literal, character))
                    report(start_line, start_column,
                           "Single quotes require one character; use double quotes for strings", "GNR0904");
            }
            emit(
                (quote == '"' || single_quoted_strings) ? TokenKind::string_literal
                             : TokenKind::character_literal,
                start,
                start_line,
                start_column
            );
            continue;
        }

        if (
            std::isalpha(static_cast<unsigned char>(current)) != 0 ||
            current == '_'
        ) {
            advance();

            while (index < source_.size()) {
                const char value = source_[index];
                if (
                    std::isalnum(static_cast<unsigned char>(value)) == 0 &&
                    value != '_'
                ) {
                    break;
                }

                advance();
            }

            const auto lexeme = source_.substr(start, index - start);
            emit(
                keyword(lexeme) ? TokenKind::keyword : TokenKind::identifier,
                start,
                start_line,
                start_column
            );
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(current)) != 0 ||
            (current == '.' && index + 1 < source_.size() &&
             std::isdigit(static_cast<unsigned char>(source_[index + 1])) != 0)) {
            advance();

            while (index < source_.size()) {
                const char value = source_[index];
                if (
                    std::isalnum(static_cast<unsigned char>(value)) == 0 &&
                    value != '_' &&
                    value != '.' &&
                    !((value == '+' || value == '-') && index > start &&
                      (source_[index - 1] == 'e' || source_[index - 1] == 'E') &&
                      !(index > start + 1 && source_[start] == '0' &&
                        (source_[start + 1] == 'x' || source_[start + 1] == 'X')))
                ) {
                    break;
                }

                advance();
            }

            static const std::regex number{
                R"((0[xX][0-9a-fA-F]+([uU]([lL]|ll|LL)?|([lL]|ll|LL)[uU]?)?|0[bB][01]+([uU]([lL]|ll|LL)?|([lL]|ll|LL)[uU]?)?|[0-9]+([uU]([lL]|ll|LL)?|([lL]|ll|LL)[uU]?)?|([0-9]+\.[0-9]*|\.[0-9]+|[0-9]+)([eE][+-]?[0-9]+)?[fFlL]?))"};
            std::string literal{source_.substr(start, index - start)};
            bool separators_valid = true;
            if (single_quoted_strings) {
                const bool hexadecimal = literal.starts_with("0x") || literal.starts_with("0X");
                auto digit = [&](char value) { return hexadecimal ? std::isxdigit(static_cast<unsigned char>(value)) != 0 : std::isdigit(static_cast<unsigned char>(value)) != 0; };
                for (std::size_t i = 0; i < literal.size(); ++i) if (literal[i] == '_' && (i == 0 || i + 1 == literal.size() || !digit(literal[i-1]) || !digit(literal[i+1]))) separators_valid = false;
                std::erase(literal, '_');
            }
            if (!separators_valid || !std::regex_match(literal, number))
                report(start_line, start_column, "Malformed numeric literal '" + literal + "'", "GNR0903");
            emit(TokenKind::number, start, start_line, start_column);
            continue;
        }

        advance();
        emit(TokenKind::symbol, start, start_line, start_column);
    }

    tokens.push_back(Token{
        TokenKind::end,
        {},
        source_.size(),
        line,
        column
    });

    return tokens;
}

} // namespace gungnir::language

