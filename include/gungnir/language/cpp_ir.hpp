#pragma once

#include <string>
#include <vector>

namespace gungnir::language {

class ValidatedProject;

enum class CppIrFragmentKind {
    interface_declaration,
    implementation_definition,
    module_definition
};

struct CppIrFragment {
    CppIrFragmentKind kind{CppIrFragmentKind::implementation_definition};
    std::string code;
};

struct CppIrUnit {
    std::string module;
    std::vector<CppIrFragment> fragments;
};

struct CppIrProject {
    // The monolithic interface preserves line-directive behavior for single-file
    // emission. The header interface intentionally omits line directives so
    // generated program.hpp remains stable across source-location-only changes.
    std::vector<CppIrFragment> interface_fragments;
    std::vector<CppIrFragment> header_fragments;
    std::vector<CppIrFragment> implementation_fragments;
    std::vector<CppIrUnit> units;
};

class CppIrLowerer {
public:
    [[nodiscard]] CppIrProject lower(
        const ValidatedProject& project,
        bool line_directives = true
    ) const;
};

[[nodiscard]] std::string dump_cpp_ir(const CppIrProject& project);

} // namespace gungnir::language
