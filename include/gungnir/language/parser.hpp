#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <gungnir/language/ast.hpp>
#include <gungnir/language/diagnostic.hpp>
#include <gungnir/language/token.hpp>

namespace gungnir::language {

struct ParseResult {
    Program program;
    std::vector<Diagnostic> diagnostics;
};

class Parser {
public:
    Parser(std::vector<Token> tokens, std::string source_name);

    [[nodiscard]] ParseResult parse();

private:
    [[nodiscard]] std::optional<std::size_t> next_significant(
        std::size_t index
    ) const;

    [[nodiscard]] std::optional<std::size_t> previous_significant(
        std::size_t index
    ) const;

    [[nodiscard]] bool statement_start(std::size_t index) const;
    [[nodiscard]] bool declared(const std::string& name) const;
    [[nodiscard]] bool declared_here(const std::string& name) const;

    void declare(std::string name);
    void register_explicit_declaration(std::size_t index);
    void add_duplicate_diagnostic(const Token& token, const std::string& name);

    std::vector<Token> tokens_;
    std::string source_name_;
    std::vector<std::unordered_set<std::string>> scopes_;
    ParseResult result_;
};

} // namespace gungnir::language
