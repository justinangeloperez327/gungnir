#include <gungnir/language/semantic.hpp>
#include <gungnir/language/type_system.hpp>

#include <string>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace gungnir::language {

namespace {

Type scalar_type(std::string_view name) {
    if (name == "string") return {TypeKind::string, "string", false};
    if (name == "bool" || name == "boolean") {
        return {TypeKind::boolean, "bool", false};
    }
    if (name == "int" || name == "integer" || name == "int64" ||
        name == "uint64") return {TypeKind::integer, "int", false};
    if (name == "float" || name == "double") {
        return {TypeKind::decimal, "decimal", false};
    }
    return {};
}

} // namespace

void SemanticIndex::add(const Program& program) {
    for (const auto& node : program.nodes) {
        const auto* base = std::get_if<FrameworkBase>(&node);
        const auto* framework = std::get_if<FrameworkDeclaration>(&node);
        if (!base && !framework) continue;
        const auto& name = base ? base->class_name : framework->class_name;
        const auto kind = base ? base->kind : framework->kind;
        const auto& members = base ? base->members : framework->members;
        types.emplace(name, kind);
        if (kind != FrameworkBaseKind::controller) continue;
        for (const auto index : members) {
            const auto& member = program.nodes[index];
            if (const auto* method = std::get_if<ControllerMethod>(&member)) {
                actions[name].insert(method->name);
            }
        }
    }
}

std::vector<Diagnostic> SemanticAnalyzer::analyze(
    const Program& program, std::string_view source_name,
    const SemanticIndex* project
) const {
    std::vector<Diagnostic> diagnostics;
    SemanticIndex index = project ? *project : SemanticIndex{};
    index.add(program);
    const auto& types = index.types;
    auto& actions = index.actions;

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
                } else if (project && project->closed_world &&
                           found == types.end()) {
                    report(span, "Unknown related model '" +
                                 relation->related_type + "'", "GNR1308");
                }
                if (project && project->closed_world &&
                    !relation->through_type.empty() &&
                    !types.contains(relation->through_type)) {
                    report(span, "Unknown through model '" +
                                 relation->through_type + "'", "GNR1308");
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
            std::unordered_map<std::string, Type> local_types;
            for (const auto& parameter : *parameters) {
                local_types.insert_or_assign(
                    parameter.name, scalar_type(parameter.type_name)
                );
            }
            TypeSystem type_system;
            std::function<Type(const Expression&)> infer =
                [&](const Expression& expression) -> Type {
                if (expression.kind == ExpressionKind::literal)
                    return type_system.infer_literal(expression.text);
                if (expression.kind == ExpressionKind::name) {
                    const auto found = local_types.find(expression.text);
                    return found == local_types.end() ? Type{} : found->second;
                }
                if (expression.kind == ExpressionKind::group &&
                    !expression.arguments.empty()) return infer(expression.arguments.front());
                if (expression.kind == ExpressionKind::unary &&
                    !expression.arguments.empty()) {
                    const auto operand = infer(expression.arguments.front());
                    if (expression.text == "!")
                        return operand.kind == TypeKind::boolean
                            ? Type{TypeKind::boolean, "bool", false} : Type{};
                    if (expression.text == "-" || expression.text == "+")
                        return operand.kind == TypeKind::integer ||
                               operand.kind == TypeKind::decimal ? operand : Type{};
                }
                if (expression.kind == ExpressionKind::binary &&
                    expression.arguments.size() == 2) {
                    const auto lhs = infer(expression.arguments[0]);
                    const auto rhs = infer(expression.arguments[1]);
                    const auto& op = expression.text;
                    if (op == "=") return rhs;
                    if (!lhs.known() || !rhs.known()) return {};
                    if (op == "==" || op == "!=" || op == "<" || op == ">" ||
                        op == "<=" || op == ">=" || op == "&&" || op == "||")
                        return {TypeKind::boolean, "bool", false};
                    if (op == "+" && lhs.kind == TypeKind::string &&
                        rhs.kind == TypeKind::string) return lhs;
                    const bool lhs_numeric = lhs.kind == TypeKind::integer ||
                                             lhs.kind == TypeKind::decimal;
                    const bool rhs_numeric = rhs.kind == TypeKind::integer ||
                                             rhs.kind == TypeKind::decimal;
                    if (lhs_numeric && rhs_numeric)
                        return lhs.kind == TypeKind::decimal || rhs.kind == TypeKind::decimal
                            ? Type{TypeKind::decimal, "decimal", false}
                            : lhs;
                }
                return {};
            };
            for (const auto& statement : *body) {
                const Type value = infer(statement.expression);
                if (statement.kind == StatementKind::binding &&
                    !statement.name.empty()) {
                    if (local_types.contains(statement.name)) {
                        report(statement.span, "Duplicate local binding '" +
                               statement.name + "'", "GNR1310");
                    }
                    local_types.insert_or_assign(statement.name, value);
                }
                if (statement.kind != StatementKind::return_) continue;
                if (return_type == "void") {
                    report(statement.span,
                           "Void method cannot return a value", "GNR1304");
                } else if (value.known() &&
                           !type_system.assignable(scalar_type(return_type), value)) {
                    report(statement.span,
                           "Return value does not match '" + return_type +
                               "'", "GNR1306");
                }
            }
        }
    }

    for (const auto& node : program.nodes) {
        const auto* route = std::get_if<RouteDeclaration>(&node);
        if (!route) continue;
        const auto found = types.find(route->controller_name);
        if (project && project->closed_world && found == types.end()) {
            report(route->route_span, "Unknown controller '" +
                        route->controller_name + "'", "GNR1309");
        } else if (found != types.end() && found->second != FrameworkBaseKind::controller) {
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
