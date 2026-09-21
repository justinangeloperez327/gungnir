#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <gungnir/language/diagnostic.hpp>
#include <gungnir/language/model_lowering.hpp>

namespace gungnir::language {

struct AsyncLoweringResult {
    std::vector<SourceEdit> edits;
    std::vector<Diagnostic> diagnostics;
};

class AsyncLowerer {
public:
    [[nodiscard]] AsyncLoweringResult lower(
        std::string_view source,
        std::string source_name = "<memory>"
    ) const;
};

} // namespace gungnir::language
