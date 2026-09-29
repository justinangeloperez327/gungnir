#pragma once

#include <string_view>
#include <vector>

#include <gungnir/language/ast.hpp>
#include <gungnir/language/diagnostic.hpp>

namespace gungnir::language {

class SemanticAnalyzer {
public:
    [[nodiscard]] std::vector<Diagnostic> analyze(
        const Program& program,
        std::string_view source_name = "<memory>"
    ) const;
};

} // namespace gungnir::language
