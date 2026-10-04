#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace gungnir::language {

class ValidatedProject;

using CppIrId = std::size_t;
inline constexpr CppIrId invalid_cpp_ir_id =
    static_cast<CppIrId>(-1);

struct CppIrType {
    std::string spelling;

    [[nodiscard]] bool valid() const noexcept {
        return !spelling.empty();
    }
};

struct CppIrSource {
    std::string file;
    std::size_t line{1};
    std::size_t column{1};
};

enum class CppIrExpressionKind {
    literal,
    name,
    member,
    call,
    unary,
    binary,
    subscript,
    list,
    object,
    lambda,
    await_,
    conditional,
    conversion
};

struct CppIrExpression {
    CppIrExpressionKind kind{CppIrExpressionKind::literal};
    CppIrType type;
    // Final target spelling is cached by lowering. It is never source text and
    // never used to rediscover Gungnir semantics.
    std::string spelling;
    std::vector<CppIrId> operands;
    std::vector<CppIrId> body;
};

enum class CppIrStatementKind {
    binding,
    expression,
    return_,
    co_return_,
    throw_,
    block,
    if_,
    while_,
    for_,
    for_in,
    break_,
    continue_,
    route_registration
};

struct CppIrStatement {
    CppIrStatementKind kind{CppIrStatementKind::expression};
    CppIrSource source;
    std::string name;
    CppIrType type;
    bool immutable{false};
    CppIrId expression{invalid_cpp_ir_id};
    std::vector<CppIrId> body;
    std::vector<CppIrId> alternative;
    std::vector<CppIrId> parts;
    CppIrId route{invalid_cpp_ir_id};
};

enum class CppIrRouteMethod { get, post, put, patch, delete_, options, head, fallback };
enum class CppIrRouteBinding { request, scalar, model };
struct CppIrRouteParameter { CppIrType type; std::string name; CppIrRouteBinding binding{CppIrRouteBinding::scalar}; };
struct CppIrRouteMiddleware { CppIrType type; std::string alias; };
struct CppIrRouteConstraint { std::string parameter, expression; };
struct CppIrRoute {
    CppIrRouteMethod method{CppIrRouteMethod::get};
    std::string path, name, action;
    CppIrType controller;
    std::vector<CppIrRouteParameter> parameters;
    std::vector<CppIrRouteMiddleware> middleware;
    std::vector<CppIrRouteConstraint> constraints;
};

struct CppIrParameter {
    CppIrType type;
    std::string name;
    bool by_reference{false};
    CppIrId default_value{invalid_cpp_ir_id};
};

struct CppIrFunction {
    std::string module;
    std::string owner;
    std::string name;
    CppIrType result;
    bool coroutine{false};
    bool line_directive{true};
    CppIrSource source;
    std::vector<CppIrParameter> parameters;
    std::vector<CppIrId> body;
};

enum class CppIrDeclarationKind {
    preamble,
    function_forward,
    class_forward,
    class_definition,
    model_metadata
};

struct CppIrDeclaration {
    CppIrDeclarationKind kind{CppIrDeclarationKind::preamble};
    std::string module;
    std::string name;
    std::string spelling;
};

struct CppIrUnit {
    std::string module;
    std::vector<std::size_t> functions;
};

struct CppIrProject {
    // Interface output is ordered structural declaration IR. Executable bodies
    // are typed function/statement/expression IR and never monolithic fragments.
    std::vector<CppIrDeclaration> interface_declarations;
    std::vector<CppIrDeclaration> header_declarations;
    std::vector<CppIrExpression> expressions;
    std::vector<CppIrStatement> statements;
    std::vector<CppIrFunction> functions;
    std::vector<CppIrUnit> units;
    std::vector<CppIrRoute> routes;
};

class CppIrLowerer {
public:
    [[nodiscard]] CppIrProject lower(
        const ValidatedProject& project,
        bool line_directives = true
    ) const;
};

struct CppIrVerification {
    std::vector<std::string> errors;

    [[nodiscard]] bool success() const noexcept {
        return errors.empty();
    }
};

class CppIrVerifier {
public:
    [[nodiscard]] CppIrVerification verify(
        const CppIrProject& project
    ) const;
};

[[nodiscard]] std::string dump_cpp_ir(
    const CppIrProject& project
);

} // namespace gungnir::language
