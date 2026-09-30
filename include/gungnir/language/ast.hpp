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

struct ApplicationReference {
    SourceSpan span;
};

enum class FrameworkBaseKind {
    model,
    controller,
    migration,
    middleware,
    policy,
    event,
    listener,
    notification,
    mail
};

struct FrameworkBase {
    SourceSpan span;
    std::string class_name;
    FrameworkBaseKind kind{FrameworkBaseKind::controller};
    SourceSpan declaration_span;
    std::vector<std::size_t> members;
    SourceSpan body_open_span;
};

struct MethodParameter {
    SourceSpan span;
    std::string type_name;
    std::string name;
};

enum class ExpressionKind {
    name, literal, call, raw, unary, binary, member, subscript, group,
    object, entry, list
};

struct Expression {
    SourceSpan span;
    ExpressionKind kind{ExpressionKind::raw};
    std::string text;
    std::vector<Expression> arguments;
    // For operators and postfix expressions, arguments contain operands in source order.
};

struct InferredBinding {
    SourceSpan span;
    std::string name;
    bool immutable{false};
    Expression initializer;
};

enum class StatementKind {
    return_, binding, expression, block, conditional, loop_, break_, continue_
};

struct MethodStatement {
    SourceSpan span;
    StatementKind kind{StatementKind::expression};
    Expression expression;
    std::string name;
    std::vector<MethodStatement> children;
    std::vector<MethodStatement> alternative;
    // For a for-loop: initializer, condition, and update (empty parts omitted).
    std::vector<Expression> for_parts;
    std::string for_binding_name;
    Expression for_binding_initializer;
    bool for_binding_immutable{false};
    // Native typed locals remain source-preserving but participate in lookup.
    std::string declared_name;
    std::string declared_type;
    bool declared_immutable{false};
};

struct FrameworkDeclaration {
    SourceSpan keyword_span;
    SourceSpan name_end_span;
    SourceSpan body_end_span;
    std::string class_name;
    FrameworkBaseKind kind{FrameworkBaseKind::controller};
    bool needs_semicolon{false};
    // Indices into Program::nodes, preserving the existing flat node API.
    SourceSpan span;
    std::vector<std::size_t> members;
    SourceSpan body_open_span;
};

struct FrameworkMethod {
    SourceSpan span;
    SourceSpan return_type_span;
    SourceSpan name_span;
    std::string owner_name;
    FrameworkBaseKind owner_kind{FrameworkBaseKind::controller};
    std::string return_type;
    std::string name;
    bool asynchronous{false};
    std::vector<MethodParameter> parameters;
    std::vector<MethodStatement> body;
};

struct ModelField {
    SourceSpan declaration_span;
    SourceSpan type_span;
    std::string model_name;
    std::string type_name;
    std::string name;
    bool nullable{false};
    bool primary_key{false};
    bool shorthand{false};
};

enum class ModelConfigurationKind {
    table,
    connection,
    timestamps,
    soft_deletes
};

struct ModelConfiguration {
    SourceSpan span;
    std::string model_name;
    ModelConfigurationKind kind{ModelConfigurationKind::table};
    std::string value;
    bool enabled{false};
};

enum class ModelRelationshipKind {
    has_one,
    has_many,
    belongs_to,
    belongs_to_many,
    has_one_through,
    has_many_through
};

struct ModelRelationship {
    SourceSpan span;
    std::string model_name;
    std::string name;
    ModelRelationshipKind kind{ModelRelationshipKind::has_many};
    std::string related_type;
    std::string through_type;
    std::vector<std::string> arguments;
};

struct ControllerMethod {
    SourceSpan span;
    SourceSpan return_type_span;
    SourceSpan name_span;
    std::string controller_name;
    std::string return_type;
    std::string name;
    bool asynchronous{false};
    std::vector<MethodParameter> parameters;
    std::vector<MethodStatement> body;
};

struct InjectDeclaration {
    SourceSpan span;
    SourceSpan type_span;
    SourceSpan name_span;
    std::string controller_name;
    std::string type_name;
    std::string name;
};

enum class RouteMethodKind {
    get,
    post,
    put,
    patch,
    remove,
    options,
    head
};

struct RouteDeclaration {
    SourceSpan route_span;
    SourceSpan method_span;
    SourceSpan handler_prefix_span;
    SourceSpan middleware_span;
    RouteMethodKind method{RouteMethodKind::get};
    std::string controller_name;
    std::string action_name;
    std::string middleware_type;
    bool has_middleware{false};
};

using Node = std::variant<
    InferredBinding,
    ApplicationReference,
    FrameworkBase,
    FrameworkDeclaration,
    FrameworkMethod,
    ModelField,
    ModelConfiguration,
    ModelRelationship,
    ControllerMethod,
    InjectDeclaration,
    RouteDeclaration
>;

struct Program {
    std::vector<Node> nodes;
};

} // namespace gungnir::language

