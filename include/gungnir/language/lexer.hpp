#pragma once

#include <string_view>
#include <vector>

#include <gungnir/language/token.hpp>

namespace gungnir::language {

class Lexer {
public:
    explicit Lexer(std::string_view source) noexcept;

    [[nodiscard]] std::vector<Token> tokenize() const;

private:
    std::string_view source_;
};

} // namespace gungnir::language
