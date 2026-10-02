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
    co_return,
    throw_,
    block,
    if_,
    while_,
    for_,
    for_in,
    break_,
    continue_
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
    // Framework-generated class declarations and metadata remain explicit
    // support blocks. User-authored executable bodies are represented by typed
    // function/statement/expression IR and never as monolithic text fragments.
    std::vector<CppIrSupportBlock> support;
    std::vector<CppIrExpression> expressions;
    std::vector<CppIrStatement> statements;
    std::vector<CppIrFunction> functions;
    std::vector<CppIrUnit> units;
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
