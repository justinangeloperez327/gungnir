#include <gungnir/language/cpp_ir.hpp>
#include <gungnir/language/compiler.hpp>

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace gungnir::language {
namespace {

std::string quote(std::string_view value) {
    std::ostringstream out;
    out << std::quoted(std::string{value});
    return out.str();
}

std::string cpp_namespace(std::string_view module) {
    if (module.empty()) {
        return {};
    }

    std::string result{"gnr::"};
    for (char value : module) {
        if (value == '.') {
            result += "::";
        } else {
            result += value;
        }
    }

    return result;
}

const CppIrSupportBlock* support(
    const CppIrProject& project,
    CppIrSupportKind kind
) {
    for (const auto& block : project.support) {
        if (block.kind == kind) {
            return &block;
        }
    }
    return nullptr;
}

class StructuralEmitter {
public:
    explicit StructuralEmitter(
        const CppIrProject& project
    ) : project_(project) {}

    std::string expression(CppIrId id) const {
        if (id == invalid_cpp_ir_id) {
            return {};
        }

        return project_.expressions.at(id).spelling;
    }

    std::string statement(CppIrId id) const {
        const auto& node = project_.statements.at(id);

        switch (node.kind) {
        case CppIrStatementKind::binding:
            return std::string{node.immutable ? "const " : ""} +
                node.type.spelling + " " + node.name +
                " = " + expression(node.expression) + ";\n";
        case CppIrStatementKind::expression:
            return expression(node.expression) + ";\n";
        case CppIrStatementKind::return_:
            return std::string{"return"} +
                (node.expression == invalid_cpp_ir_id
                    ? ""
                    : " " + expression(node.expression)) +
                ";\n";
        case CppIrStatementKind::co_return:
            return std::string{"co_return"} +
                (node.expression == invalid_cpp_ir_id
                    ? ""
                    : " " + expression(node.expression)) +
                ";\n";
        case CppIrStatementKind::throw_:
            return "throw " + expression(node.expression) + ";\n";
        case CppIrStatementKind::block:
            return block(node.body);
        case CppIrStatementKind::if_:
            return "if (" + expression(node.expression) + ") " +
                block(node.body) +
                (node.alternative.empty()
                    ? ""
                    : "else " + block(node.alternative));
        case CppIrStatementKind::while_:
            return "while (" + expression(node.expression) + ") " +
                block(node.body);
        case CppIrStatementKind::for_in:
            return "for (const auto& " + node.name + " : " +
                expression(node.expression) + ") " +
                block(node.body);
        case CppIrStatementKind::for_: {
            std::string result{"{\n"};

            for (auto part : node.parts) {
                result += statement(part);
            }

            result += "for (;" + expression(node.expression) + ";";

            bool comma = false;
            for (auto step : node.alternative) {
                const auto& step_node =
                    project_.statements.at(step);

                if (step_node.kind !=
                    CppIrStatementKind::expression) {
                    throw std::logic_error(
                        "C++ IR for-loop step is not an expression"
                    );
                }

                if (comma) {
                    result += ',';
                }
                comma = true;
                result += expression(step_node.expression);
            }

            result += ") " + block(node.body) + "}\n";
            return result;
        }
        case CppIrStatementKind::break_:
            return "break;\n";
        case CppIrStatementKind::continue_:
            return "continue;\n";
        }

        throw std::logic_error(
            "Unknown C++ IR statement kind"
        );
    }

    std::string function(std::size_t id) const {
        const auto& value = project_.functions.at(id);
        std::string result;

        const auto ns = cpp_namespace(value.module);
        if (!ns.empty()) {
            result += "namespace " + ns + " {\n";
        }

        if (
            value.line_directive &&
            !value.source.file.empty()
        ) {
            result += "#line " +
                std::to_string(value.source.line) +
                " " + quote(value.source.file) + "\n";
        }

        result += value.result.spelling + " ";

        if (!value.owner.empty()) {
            result += value.owner + "::";
        }

        result += value.name + "(";

        for (
            std::size_t i = 0;
            i < value.parameters.size();
            ++i
        ) {
            if (i) {
                result += ',';
            }

            const auto& parameter =
                value.parameters[i];
            result += parameter.type.spelling;
            result += parameter.by_reference ? "& " : " ";
            result += parameter.name;
        }

        result += ") {\n";
        for (auto statement_id : value.body) {
            result += statement(statement_id);
        }
        result += "}\n";

        if (!ns.empty()) {
            result += "}\n";
        }

        return result;
    }

private:
    std::string block(
        const std::vector<CppIrId>& statements
    ) const {
        std::string result{"{\n"};
        for (auto id : statements) {
            result += statement(id);
        }
        result += "}\n";
        return result;
    }

    const CppIrProject& project_;
};

void enforce_valid_ir(const CppIrProject& project) {
    const auto verification =
        CppIrVerifier{}.verify(project);

    if (!verification.success()) {
        std::string message{
            "Gungnir internal compiler error: invalid C++ IR"
        };

        for (const auto& error : verification.errors) {
            message += "\n - " + error;
        }

        throw std::logic_error(message);
    }
}

} // namespace

std::string CppEmitter::emit(
    const CppIrProject& project
) const {
    enforce_valid_ir(project);

    std::string output;

    if (const auto* block = support(
        project,
        CppIrSupportKind::interface_
    )) {
        output += block->spelling;
    }

    StructuralEmitter emitter{project};
    for (
        std::size_t i = 0;
        i < project.functions.size();
        ++i
    ) {
        output += emitter.function(i);
    }

    return output;
}

EmittedProject CppEmitter::emit_units(
    const CppIrProject& project
) const {
    enforce_valid_ir(project);

    EmittedProject result;
    result.declarations = "#pragma once\n";

    if (const auto* block = support(
        project,
        CppIrSupportKind::header_interface
    )) {
        result.declarations += block->spelling;
    }

    StructuralEmitter emitter{project};

    result.units.reserve(project.units.size());
    for (const auto& unit : project.units) {
        EmittedUnit emitted;
        emitted.module = unit.module;
        emitted.code = "#include \"program.hpp\"\n";

        for (auto function : unit.functions) {
            emitted.code += emitter.function(function);
        }

        result.units.push_back(std::move(emitted));
    }

    return result;
}

std::string CppEmitter::emit(
    const ValidatedProject& project,
    bool line_directives
) const {
    return emit(
        CppIrLowerer{}.lower(
            project,
            line_directives
        )
    );
}

EmittedProject CppEmitter::emit_units(
    const ValidatedProject& project,
    bool line_directives
) const {
    return emit_units(
        CppIrLowerer{}.lower(
            project,
            line_directives
        )
    );
}

} // namespace gungnir::language
