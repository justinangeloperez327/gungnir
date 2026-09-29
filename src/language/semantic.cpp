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

void SemanticIndex::add(const Program& program, std::string_view source_name) {
    for (const auto& node : program.nodes) {
        const auto* base = std::get_if<FrameworkBase>(&node);
        const auto* framework = std::get_if<FrameworkDeclaration>(&node);
        if (!base && !framework) continue;
        const auto& name = base ? base->class_name : framework->class_name;
        const auto kind = base ? base->kind : framework->kind;
        const auto& members = base ? base->members : framework->members;
        types.emplace(name, kind);
        declaration_sources[name].insert(std::string{source_name});
        for (const auto member_index : members) {
            const auto& member = program.nodes[member_index];
            if (const auto* method = std::get_if<ControllerMethod>(&member)) {
                actions[name].insert(method->name);
                MethodSignature signature;
                signature.return_type = method->return_type;
                for (const auto& parameter : method->parameters)
                    signature.parameters.push_back(parameter.type_name);
                methods[name + "::" + method->name].push_back(std::move(signature));
            } else if (const auto* method = std::get_if<FrameworkMethod>(&member)) {
                MethodSignature signature;
                signature.return_type = method->return_type;
                for (const auto& parameter : method->parameters)
                    signature.parameters.push_back(parameter.type_name);
                methods[name + "::" + method->name].push_back(std::move(signature));
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
    index.add(program, source_name);
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

    std::unordered_set<std::string> declarations_in_file;
    for (const auto& node : program.nodes) {
        const auto* base = std::get_if<FrameworkBase>(&node);
        const auto* framework = std::get_if<FrameworkDeclaration>(&node);
        if (!base && !framework) continue;
        const auto& members = base ? base->members : framework->members;
        const auto& declaration_name = base
            ? base->class_name : framework->class_name;
        const auto declaration_kind = base ? base->kind : framework->kind;
        if (!declarations_in_file.insert(declaration_name).second) {
            report(base ? base->span : framework->span,
                   "Duplicate framework declaration '" + declaration_name +
                       "' in this file", "GNR1314");
        }
        const auto& declared_in = index.declaration_sources.at(declaration_name);
        if (declared_in.size() > 1 &&
            declared_in.contains(std::string{source_name})) {
            report(base ? base->span : framework->span,
                   "Duplicate framework declaration '" + declaration_name +
                       "' across project files", "GNR1314");
        }

        std::unordered_set<std::string> member_names;
        std::unordered_set<std::string> method_signatures;
        std::unordered_set<std::string> method_names;
        for (const auto member_index : members) {
            const auto& member = program.nodes[member_index];
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
                if (!relation->through_type.empty()) {
                    const auto through = types.find(relation->through_type);
                    if (through != types.end() &&
                        through->second != FrameworkBaseKind::model) {
                        report(span, "Through relationship type '" +
                                     relation->through_type + "' is not a model",
                               "GNR1303");
                    } else if (project && project->closed_world &&
                               through == types.end()) {
                        report(span, "Unknown through model '" +
                                     relation->through_type + "'", "GNR1308");
                    }
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
            std::unordered_set<std::string> immutable_names;
            std::function<Type(const Expression&)> infer =
                [&](const Expression& expression) -> Type {
                if (expression.kind == ExpressionKind::literal)
                    return type_system.infer_literal(expression.text);
                if (expression.kind == ExpressionKind::name) {
                    const auto found = local_types.find(expression.text);
                    return found == local_types.end() ? Type{} : found->second;
                }
                if (expression.kind == ExpressionKind::call &&
                    !expression.arguments.empty()) {
                    const auto& callee = expression.arguments.front();
                    std::string owner = declaration_name;
                    std::string method_name;
                    if (callee.kind == ExpressionKind::name) {
                        method_name = callee.text;
                    } else if (callee.kind == ExpressionKind::member &&
                               callee.text == "::" &&
                               callee.arguments.size() == 2 &&
                               callee.arguments[0].kind == ExpressionKind::name &&
                               callee.arguments[1].kind == ExpressionKind::name) {
                        owner = callee.arguments[0].text;
                        method_name = callee.arguments[1].text;
                    }
                    const auto found = index.methods.find(owner + "::" + method_name);
                    if (method_name.empty() || found == index.methods.end()) return {};
                    const auto count = expression.arguments.size() - 1;
                    bool matching_arity = false;
                    for (const auto& signature : found->second) {
                        if (signature.parameters.size() != count) continue;
                        matching_arity = true;
                        bool compatible = true;
                        for (std::size_t argument = 0; argument < count; ++argument) {
                            if (!type_system.assignable(
                                    scalar_type(signature.parameters[argument]),
                                    infer(expression.arguments[argument + 1]))) {
                                compatible = false;
                            }
                        }
                        if (compatible) return scalar_type(signature.return_type);
                    }
                    report(expression.span,
                           matching_arity ? "Call argument type mismatch" :
                                            "Call argument count mismatch",
                           matching_arity ? "GNR1318" : "GNR1317");
                    return {};
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
                    if (op == "=") {
                        if (expression.arguments[0].kind == ExpressionKind::name &&
                            immutable_names.contains(expression.arguments[0].text)) {
                            report(expression.span, "Cannot assign to constant '" +
                                   expression.arguments[0].text + "'", "GNR1320");
                        }
                        if (lhs.known() && rhs.known() &&
                            !type_system.assignable(lhs, rhs))
                            report(expression.span, "Assignment type mismatch", "GNR1316");
                        return rhs;
                    }
                    if (!lhs.known() || !rhs.known()) return {};
                    const bool lhs_numeric = lhs.kind == TypeKind::integer ||
                                             lhs.kind == TypeKind::decimal;
                    const bool rhs_numeric = rhs.kind == TypeKind::integer ||
                                             rhs.kind == TypeKind::decimal;
                    if (op == "&&" || op == "||") {
                        if (lhs.kind == TypeKind::boolean &&
                            rhs.kind == TypeKind::boolean)
                            return {TypeKind::boolean, "bool", false};
                        report(expression.span, "Logical operands must be boolean", "GNR1316");
                        return {};
                    }
                    if (op == "==" || op == "!=") {
                        if (type_system.assignable(lhs, rhs) ||
                            type_system.assignable(rhs, lhs))
                            return {TypeKind::boolean, "bool", false};
                        report(expression.span, "Incompatible comparison operands", "GNR1316");
                        return {};
                    }
                    if (op == "<" || op == ">" || op == "<=" || op == ">=") {
                        if ((lhs_numeric && rhs_numeric) ||
                            (lhs.kind == TypeKind::string && rhs.kind == TypeKind::string))
                            return {TypeKind::boolean, "bool", false};
                        report(expression.span, "Incompatible comparison operands", "GNR1316");
                        return {};
                    }
                    if (op == "+" && lhs.kind == TypeKind::string &&
                        rhs.kind == TypeKind::string) return lhs;
                    if (lhs_numeric && rhs_numeric)
                        return lhs.kind == TypeKind::decimal || rhs.kind == TypeKind::decimal
                            ? Type{TypeKind::decimal, "decimal", false}
                            : lhs;
                    if (op == "+" || op == "-" || op == "*" || op == "/" ||
                        op == "%")
                        report(expression.span, "Arithmetic operands must be numeric", "GNR1316");
                }
                return {};
            };
            std::function<void(const std::vector<MethodStatement>&,
                               std::unordered_map<std::string, Type>&, unsigned,
                               std::unordered_set<std::string>,
                               std::unordered_set<std::string>)> check_body =
                [&](const std::vector<MethodStatement>& statements,
                    std::unordered_map<std::string, Type>& scope, unsigned loop_depth,
                    std::unordered_set<std::string> declared_here,
                    std::unordered_set<std::string> constants) {
            for (const auto& statement : statements) {
                if ((statement.kind == StatementKind::break_ ||
                     statement.kind == StatementKind::continue_) && loop_depth == 0) {
                    report(statement.span, "Loop control outside a loop", "GNR1313");
                }
                local_types = scope;
                immutable_names = constants;
                const bool has_expression = statement.expression.span.end >
                                            statement.expression.span.begin;
                const Type value = has_expression ? infer(statement.expression) : Type{};
                if (statement.kind == StatementKind::conditional ||
                    statement.kind == StatementKind::loop_) {
                    if (value.known() &&
                        value.kind != TypeKind::boolean) {
                        report(statement.expression.span,
                               "Control-flow condition must be boolean", "GNR1311");
                    }
                }
                if (statement.kind == StatementKind::binding &&
                    !statement.name.empty()) {
                    if (!declared_here.insert(statement.name).second) {
                        report(statement.span, "Duplicate local binding '" +
                               statement.name + "'", "GNR1310");
                    }
                    scope.insert_or_assign(statement.name, value);
                    constants.insert(statement.name);
                }
                if (!statement.children.empty()) {
                    auto nested = scope;
                    check_body(statement.children, nested,
                               loop_depth + (statement.kind == StatementKind::loop_),
                               {}, constants);
                }
                if (!statement.alternative.empty()) {
                    auto nested = scope;
                    check_body(statement.alternative, nested, loop_depth, {}, constants);
                }
                if (statement.kind != StatementKind::return_) continue;
                if (return_type == "void") {
                    if (has_expression)
                        report(statement.span,
                               "Void method cannot return a value", "GNR1304");
                } else if (!has_expression) {
                    report(statement.span,
                           "Non-void method must return a value", "GNR1312");
                } else if (value.known() &&
                           !type_system.assignable(scalar_type(return_type), value)) {
                    report(statement.span,
                           "Return value does not match '" + return_type +
                               "'", "GNR1306");
                }
            }
            };
            auto method_scope = local_types;
            check_body(*body, method_scope, 0, parameter_names, {});
            // A loop may never run; only an unconditional return or both
            // branches of a conditional guarantee a returned value.
            std::function<bool(const std::vector<MethodStatement>&)> returns_on_all_paths =
                [&](const std::vector<MethodStatement>& statements) {
                    for (const auto& statement : statements) {
                        if (statement.kind == StatementKind::return_) return true;
                        if (statement.kind == StatementKind::block &&
                            returns_on_all_paths(statement.children)) return true;
                        if (statement.kind == StatementKind::conditional &&
                            !statement.alternative.empty() &&
                            returns_on_all_paths(statement.children) &&
                            returns_on_all_paths(statement.alternative)) return true;
                    }
                    return false;
                };
            std::function<bool(const std::vector<MethodStatement>&)> contains_return =
                [&](const std::vector<MethodStatement>& statements) {
                    for (const auto& statement : statements) {
                        if (statement.kind == StatementKind::return_ ||
                            contains_return(statement.children) ||
                            contains_return(statement.alternative)) return true;
                    }
                    return false;
                };
            if (return_type != "void" && contains_return(*body) &&
                !returns_on_all_paths(*body)) {
                report(span, "Non-void method may finish without returning a value",
                       "GNR1319");
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
