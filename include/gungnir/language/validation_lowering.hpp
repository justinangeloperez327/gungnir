#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <gungnir/language/diagnostic.hpp>
#include <gungnir/language/model_lowering.hpp>

namespace gungnir::language {

struct ValidationLoweringResult {
    std::vector<SourceEdit> edits;
    std::vector<Diagnostic> diagnostics;
};

class ValidationLowerer {
public:
    [[nodiscard]] ValidationLoweringResult lower(
        std::string_view source,
        std::string source_name = "<memory>"
    ) const;
};

} // namespace gungnir::language
