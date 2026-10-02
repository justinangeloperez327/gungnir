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

struct DiagnosticSpan {
    // Half-open byte offsets in the original .gnr source.
    std::size_t begin_offset{0};
    std::size_t end_offset{0};

    // End position is also half-open. A zero end_line means it has not yet
    // been enriched from source text.
    std::size_t end_line{0};
    std::size_t end_column{0};
    bool valid{false};
};

struct Diagnostic {
    DiagnosticLevel level{DiagnosticLevel::error};
    SourceLocation location;
    std::string message;
    std::string code;
    std::string hint;

    // Phase 7 metadata used by CLI/LSP/tooling. These fields are intentionally
    // trailing so existing aggregate initialization remains source compatible.
    DiagnosticSpan span;
    std::string source_line;
};

} // namespace gungnir::language
