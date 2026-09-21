#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <gungnir/language/model_lowering.hpp>

namespace gungnir::language {

struct BootstrapLoweringResult {
    std::vector<SourceEdit> edits;
};

class BootstrapLowerer {
public:
    [[nodiscard]] BootstrapLoweringResult lower(
        std::string_view source
    ) const;
};

} // namespace gungnir::language
