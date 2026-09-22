#pragma once

#include <cstddef>
#include <string>

namespace gungnir::language {

enum class DiagnosticLevel {
    warning,
    error
};

struct SourceLocation {
    std::string file;
    std::size_t line{1};
    std::size_t column{1};
};

struct Diagnostic {
    DiagnosticLevel level{DiagnosticLevel::error};
    SourceLocation location;
    std::string message;
    std::string code;
    std::string hint;
};

} // namespace gungnir::language
