#pragma once

#include <cstddef>
#include <string>

namespace gungnir::language {

enum class TokenKind {
    identifier,
    number,
    string_literal,
    character_literal,
    whitespace,
    comment,
    symbol,
    end
};

struct Token {
    TokenKind kind{TokenKind::end};
    std::string lexeme;
    std::size_t offset{0};
    std::size_t line{1};
    std::size_t column{1};

    [[nodiscard]] bool trivia() const noexcept {
        return kind == TokenKind::whitespace || kind == TokenKind::comment;
    }
};

} // namespace gungnir::language
