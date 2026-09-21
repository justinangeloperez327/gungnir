#pragma once

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace gungnir::language {

struct SourceSpan {
    std::size_t begin{0};
    std::size_t end{0};
    std::size_t line{1};
    std::size_t column{1};
};

struct InferredBinding {
    SourceSpan span;
    std::string name;
    bool immutable{false};
};

enum class FrameworkBaseKind {
    model,
    controller,
    migration,
    middleware
};

struct FrameworkBase {
    SourceSpan span;
    std::string class_name;
    FrameworkBaseKind kind{FrameworkBaseKind::controller};
};

using Node = std::variant<InferredBinding, FrameworkBase>;

struct Program {
    std::vector<Node> nodes;
};

} // namespace gungnir::language
