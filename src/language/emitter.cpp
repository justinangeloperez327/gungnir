#include <gungnir/language/cpp_ir.hpp>
#include <gungnir/language/compiler.hpp>

#include <string_view>
#include <utility>

namespace gungnir::language {
namespace {

void append_fragments(
    std::string& output,
    const std::vector<CppIrFragment>& fragments
) {
    for (const auto& fragment : fragments) {
        output += fragment.code;
    }
}

} // namespace

std::string CppEmitter::emit(const CppIrProject& project) const {
    std::string output;
    append_fragments(output, project.interface_fragments);
    append_fragments(output, project.implementation_fragments);
    return output;
}

EmittedProject CppEmitter::emit_units(const CppIrProject& project) const {
    EmittedProject result;
    result.declarations = "#pragma once\n";
    append_fragments(result.declarations, project.header_fragments);

    result.units.reserve(project.units.size());
    for (const auto& unit : project.units) {
        EmittedUnit emitted;
        emitted.module = unit.module;
        emitted.code = "#include \"program.hpp\"\n";
        append_fragments(emitted.code, unit.fragments);
        result.units.push_back(std::move(emitted));
    }

    return result;
}

std::string CppEmitter::emit(
    const ValidatedProject& project,
    bool line_directives
) const {
    return emit(CppIrLowerer{}.lower(project, line_directives));
}

EmittedProject CppEmitter::emit_units(
    const ValidatedProject& project,
    bool line_directives
) const {
    return emit_units(CppIrLowerer{}.lower(project, line_directives));
}

} // namespace gungnir::language
