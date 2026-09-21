#include <gungnir/language/lexer.hpp>

#include <cctype>
#include <string>

namespace gungnir::language {

Lexer::Lexer(std::string_view source) noexcept : source_(source) {}

std::vector<Token> Lexer::tokenize() const {
    std::vector<Token> tokens;

    std::size_t index = 0;
    std::size_t line = 1;
    std::size_t column = 1;

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

            while (index < source_.size()) {
                if (
                    source_[index] == '*' &&
                    index + 1 < source_.size() &&
                    source_[index + 1] == '/'
                ) {
                    advance();
                    advance();
                    break;
                }

                advance();
            }

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

            const auto delimiter_start = index;
            while (index < source_.size() && source_[index] != '(') {
                advance();
            }

            if (index < source_.size()) {
                const std::string delimiter{
                    source_.substr(delimiter_start, index - delimiter_start)
                };
                advance();

                const std::string closing = ")" + delimiter + "\"";
                const auto closing_at = source_.find(closing, index);

                if (closing_at == std::string_view::npos) {
                    while (index < source_.size()) {
                        advance();
                    }
                } else {
                    const auto end = closing_at + closing.size();
                    while (index < end) {
                        advance();
                    }
                }
            }

            emit(TokenKind::string_literal, start, start_line, start_column);
            continue;
        }

        if (current == '"' || current == '\'') {
            const char quote = current;
            bool escaped = false;
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

                if (value == quote) {
                    break;
                }
            }

            emit(
                quote == '"' ? TokenKind::string_literal
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

            emit(TokenKind::identifier, start, start_line, start_column);
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(current)) != 0) {
            advance();

            while (index < source_.size()) {
                const char value = source_[index];
                if (
                    std::isalnum(static_cast<unsigned char>(value)) == 0 &&
                    value != '_' &&
                    value != '.'
                ) {
                    break;
                }

                advance();
            }

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
