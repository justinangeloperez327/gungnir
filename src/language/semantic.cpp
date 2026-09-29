#include <gungnir/language/semantic.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace gungnir::language {

std::vector<Diagnostic> SemanticAnalyzer::analyze(
    const Program& program, std::string_view source_name
) const {
    std::vector<Diagnostic> diagnostics;
    std::unordered_map<std::string, FrameworkBaseKind> types;
    std::unordered_map<std::string, std::unordered_set<std::string>> actions;
    for (const auto& node : program.nodes) {
        if (const auto* declaration = std::get_if<FrameworkBase>(&node)) {
            types.emplace(declaration->class_name, declaration->kind);
        } else if (const auto* declaration =
                       std::get_if<FrameworkDeclaration>(&node)) {
            types.emplace(declaration->class_name, declaration->kind);
        }
    }

    const auto report = [&](SourceSpan span, std::string message,
                            std::string code) {
        diagnostics.push_back(Diagnostic{
            DiagnosticLevel::error,
            SourceLocation{std::string{source_name}, span.line, span.column},
            std::move(message), std::move(code), {}
        });
    };

    for (const auto& node : program.nodes) {
        const auto* base = std::get_if<FrameworkBase>(&node);
        const auto* framework = std::get_if<FrameworkDeclaration>(&node);
        if (!base && !framework) continue;
        const auto& members = base ? base->members : framework->members;
        const auto& declaration_name = base
            ? base->class_name : framework->class_name;
        const auto declaration_kind = base ? base->kind : framework->kind;

        std::unordered_set<std::string> member_names;
        std::unordered_set<std::string> method_signatures;
        std::unordered_set<std::string> method_names;
        for (const auto index : members) {
            const auto& member = program.nodes[index];
            std::string name;
            SourceSpan span;
            const FrameworkMethod* framework_method =
                std::get_if<FrameworkMethod>(&member);
            const ControllerMethod* controller_method =
                std::get_if<ControllerMethod>(&member);

            if (const auto* field = std::get_if<ModelField>(&member)) {
                name = field->name;
                span = field->declaration_span;
            } else if (const auto* relation =
                           std::get_if<ModelRelationship>(&member)) {
                name = relation->name;
                span = relation->span;
                const auto found = types.find(relation->related_type);
                if (found != types.end() && found->second != FrameworkBaseKind::model) {
                    report(span, "Relationship type '" + relation->related_type +
                                 "' is not a model", "GNR1303");
                }
            } else if (const auto* injection =
                           std::get_if<InjectDeclaration>(&member)) {
                name = injection->name;
                span = injection->span;
            } else if (framework_method) {
                name = framework_method->name;
                span = framework_method->name_span;
            } else if (controller_method) {
                name = controller_method->name;
                span = controller_method->name_span;
            }

            const auto* parameters = framework_method
                ? &framework_method->parameters
                : controller_method ? &controller_method->parameters : nullptr;
            const auto* body = framework_method
                ? &framework_method->body
                : controller_method ? &controller_method->body : nullptr;
            if (!name.empty()) {
                if (parameters) {
                    if (declaration_kind == FrameworkBaseKind::controller) {
                        actions[declaration_name].insert(name);
                    }
                    std::string signature = name + "(";
                    for (const auto& parameter : *parameters) {
                        signature += parameter.type_name + ",";
                    }
                    signature += ")";
                    if (!method_signatures.insert(signature).second ||
                        member_names.contains(name)) {
                        report(span, "Duplicate member '" + name + "' in '" +
                                     declaration_name + "'", "GNR1301");
                    }
                    method_names.insert(name);
                } else if (!member_names.insert(name).second ||
                           method_names.contains(name)) {
                    report(span, "Duplicate member '" + name + "' in '" +
                                 declaration_name + "'", "GNR1301");
                }
            }
            if (!parameters) continue;

            std::unordered_set<std::string> parameter_names;
            for (const auto& parameter : *parameters) {
                if (!parameter_names.insert(parameter.name).second) {
                    report(parameter.span, "Duplicate parameter '" +
                         parameter.name + "'", "GNR1302");
                }
            }
            const auto& return_type = framework_method
                ? framework_method->return_type : controller_method->return_type;
            if (return_type == "void") {
                for (const auto& statement : *body) {
                    if (statement.kind == StatementKind::return_) {
                        report(statement.span,
                               "Void method cannot return a value", "GNR1304");
                    }
                }
            } else {
                for (const auto& statement : *body) {
                    if (statement.kind != StatementKind::return_ ||
                        statement.expression.kind != ExpressionKind::literal) {
                        continue;
                    }
                    const auto& value = statement.expression.text;
                    const bool boolean = value == "true" || value == "false";
                    const bool string = !value.empty() && value.front() == '"';
                    const bool number = !value.empty() &&
                        value.front() >= '0' && value.front() <= '9';
                    const bool mismatch =
                        ((return_type == "bool" || return_type == "boolean") &&
                         !boolean) ||
                        (return_type == "string" && !string) ||
                        ((return_type == "int" || return_type == "integer" ||
                          return_type == "int64" || return_type == "uint64" ||
                          return_type == "float" || return_type == "double") &&
                         !number);
                    if (mismatch) {
                        report(statement.span,
                               "Return value does not match '" + return_type +
                                   "'", "GNR1306");
                    }
                }
            }
        }
    }

    for (const auto& node : program.nodes) {
        const auto* route = std::get_if<RouteDeclaration>(&node);
        if (!route) continue;
        const auto found = types.find(route->controller_name);
        if (found != types.end() && found->second != FrameworkBaseKind::controller) {
            report(route->route_span, "Route target '" + route->controller_name +
                        "' is not a controller", "GNR1305");
        } else if (found != types.end() &&
                   !actions[route->controller_name].contains(route->action_name)) {
            report(route->route_span, "Controller '" + route->controller_name +
                        "' has no action '" + route->action_name + "'", "GNR1307");
        }
    }
    return diagnostics;
}

} // namespace gungnir::language
