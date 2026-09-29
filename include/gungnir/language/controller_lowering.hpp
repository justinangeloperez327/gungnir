#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <gungnir/language/ast.hpp>
#include <gungnir/language/diagnostic.hpp>
#include <gungnir/language/model_lowering.hpp>
#include <gungnir/language/token.hpp>

namespace gungnir::language {

struct ControllerLoweringResult {
    std::vector<SourceEdit> edits;
    std::vector<Diagnostic> diagnostics;
};

class ControllerLowerer {
public:
    [[nodiscard]] ControllerLoweringResult lower(
        std::string_view source,
        const Program& program,
        std::string source_name = "<memory>"
    ) const;
    [[nodiscard]] ControllerLoweringResult lower(
        std::string_view source, const Program& program,
        const std::vector<Token>& tokens,
        std::string source_name = "<memory>"
    ) const;
};

} // namespace gungnir::language
