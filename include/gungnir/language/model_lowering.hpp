#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <gungnir/language/diagnostic.hpp>

namespace gungnir::language {

struct SourceEdit {
    std::size_t begin{0};
    std::size_t end{0};
    std::string replacement;
};

struct ModelLoweringResult {
    std::vector<SourceEdit> edits;
    std::vector<Diagnostic> diagnostics;
};

class ModelLowerer {
public:
    [[nodiscard]] ModelLoweringResult lower(
        std::string_view source,
        std::string source_name = "<memory>"
    ) const;
};

} // namespace gungnir::language
