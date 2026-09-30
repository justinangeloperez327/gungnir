#pragma once

#include <string_view>
#include <vector>

#include <gungnir/language/token.hpp>
#include <gungnir/language/diagnostic.hpp>

namespace gungnir::language {

class Lexer {
public:
    explicit Lexer(std::string_view source) noexcept;

    [[nodiscard]] std::vector<Token> tokenize(
        std::vector<Diagnostic>* diagnostics = nullptr,
        std::string_view source_name = "<memory>", bool single_quoted_strings = false
    ) const;

private:
    std::string_view source_;
};

} // namespace gungnir::language

