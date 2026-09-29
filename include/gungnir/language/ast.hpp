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

struct FrameworkDeclaration {
    SourceSpan keyword_span;
    SourceSpan name_end_span;
    std::string class_name;
    FrameworkBaseKind kind{FrameworkBaseKind::controller};
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
    FrameworkBase,
    FrameworkDeclaration,
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
