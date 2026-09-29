#pragma once

#include <string_view>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <gungnir/language/ast.hpp>
#include <gungnir/language/diagnostic.hpp>

namespace gungnir::language {

struct SemanticIndex {
    // Enable unresolved reference diagnostics for closed Gungnir projects.
    bool closed_world{false};
    std::unordered_map<std::string, FrameworkBaseKind> types;
    std::unordered_map<std::string, std::unordered_set<std::string>> actions;
    std::unordered_map<std::string, std::unordered_set<std::string>> declaration_sources;

    void add(const Program& program, std::string_view source_name = "<memory>");
};

class SemanticAnalyzer {
public:
    [[nodiscard]] std::vector<Diagnostic> analyze(
        const Program& program,
        std::string_view source_name = "<memory>",
        const SemanticIndex* project = nullptr
    ) const;
};

} // namespace gungnir::language
