#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <gungnir/language/diagnostic.hpp>
#include <gungnir/language/model_lowering.hpp>

namespace gungnir::language {

struct MiddlewareLoweringResult {
    std::vector<SourceEdit> edits;
    std::vector<Diagnostic> diagnostics;
};

class MiddlewareLowerer {
public:
    [[nodiscard]] MiddlewareLoweringResult lower(
        std::string_view source,
        std::string source_name = "<memory>"
    ) const;
};

} // namespace gungnir::language
