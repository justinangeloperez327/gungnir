#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <gungnir/language/diagnostic.hpp>

namespace gungnir::language {

struct TranspileOptions {
    bool emit_line_directives{true};
};

struct TranspileResult {
    std::string code;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool success() const noexcept;
};

class Transpiler {
public:
    [[nodiscard]] TranspileResult transpile(
        std::string_view source,
        std::string source_name = "<memory>",
        TranspileOptions options = {}
    ) const;
};

} // namespace gungnir::language
