#include <gungnir/language/cpp_ir.hpp>

#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace gungnir::language {
namespace {

class Verifier {
public:
    explicit Verifier(
        const CppIrProject& project
    ) : project_(project) {}

    CppIrVerification run() {
        verify_declarations();
        verify_arena_shapes();
        verify_functions();
        verify_units();

        return CppIrVerification{
            std::move(errors_)
        };
    }

private:
    void error(std::string message) {
        errors_.push_back(std::move(message));
    }

    bool expression_id(CppIrId id) {
        if (
            id == invalid_cpp_ir_id ||
            id >= project_.expressions.size()
        ) {
            error("invalid C++ IR expression id");
            return false;
        }

        return true;
    }

    bool statement_id(CppIrId id) {
        if (
            id == invalid_cpp_ir_id ||
            id >= project_.statements.size()
        ) {
            error("invalid C++ IR statement id");
            return false;
        }

        return true;
    }

    void verify_declaration_set(
        const std::vector<CppIrDeclaration>& declarations,
        std::string_view label
    ) {
        if (declarations.empty()) {
            error(
                std::string{label} +
                " C++ IR declaration set is empty"
            );
            return;
        }

        std::size_t preambles = 0;

        for (
            std::size_t id = 0;
            id < declarations.size();
            ++id
        ) {
            const auto& declaration = declarations[id];

            if (declaration.spelling.empty()) {
                error(
                    std::string{label} +
                    " C++ IR declaration has no target spelling"
                );
            }

            if (
                declaration.kind ==
                CppIrDeclarationKind::preamble
            ) {
                ++preambles;
                if (id != 0) {
                    error(
                        std::string{label} +
                        " C++ IR preamble must be first"
                    );
                }
            } else if (declaration.name.empty()) {
                error(
                    std::string{label} +
                    " C++ IR declaration has no name"
                );
            }
        }

        if (preambles != 1) {
            error(
                std::string{label} +
                " C++ IR must contain exactly one preamble"
            );
        }
    }

    void verify_declarations() {
        verify_declaration_set(
            project_.interface_declarations,
            "interface"
        );
        verify_declaration_set(
            project_.header_declarations,
            "header"
        );

        if (
            project_.interface_declarations.size() !=
            project_.header_declarations.size()
        ) {
            error(
                "C++ IR interface/header declaration shape differs"
            );
            return;
        }

        for (
            std::size_t id = 0;
            id < project_.interface_declarations.size();
            ++id
        ) {
            const auto& interface =
                project_.interface_declarations[id];
            const auto& header =
                project_.header_declarations[id];

            if (
                interface.kind != header.kind ||
                interface.module != header.module ||
                interface.name != header.name
            ) {
                error(
                    "C++ IR interface/header declaration identity differs"
                );
            }
        }
    }

    void verify_expression(
        CppIrId id,
        bool coroutine,
        std::unordered_set<CppIrId>& active_expressions,
        std::unordered_set<CppIrId>& active_statements
    ) {
        if (!expression_id(id)) {
            return;
        }

        if (!active_expressions.insert(id).second) {
            error("cycle in C++ IR expression graph");
            return;
        }

        const auto& expression =
            project_.expressions[id];

        if (!expression.type.valid()) {
            error(
                "C++ IR expression has no target type"
            );
        }

        if (expression.spelling.empty()) {
            error(
                "C++ IR expression has no target spelling"
            );
        }

        if (
            expression.kind ==
                CppIrExpressionKind::await_ &&
            !coroutine
        ) {
            error(
                "C++ IR await appears outside a coroutine"
            );
        }

        if (
            expression.kind ==
                CppIrExpressionKind::conversion &&
            expression.operands.size() != 1
        ) {
            error(
                "C++ IR conversion must have one operand"
            );
        }

        if (
            expression.kind ==
                CppIrExpressionKind::await_ &&
            expression.operands.size() != 1
        ) {
            error(
                "C++ IR await must have one operand"
            );
        }

        for (auto operand : expression.operands) {
            verify_expression(
                operand,
                coroutine,
                active_expressions,
                active_statements
            );
        }

        if (
            expression.kind ==
            CppIrExpressionKind::lambda
        ) {
            for (auto statement : expression.body) {
                verify_statement(
                    statement,
                    false,
                    0,
                    active_expressions,
                    active_statements
                );
            }
        } else if (!expression.body.empty()) {
            error(
                "only lambda C++ IR expressions may own statements"
            );
        }

        active_expressions.erase(id);
    }

    void verify_statement(
        CppIrId id,
        bool coroutine,
        std::size_t loop_depth,
        std::unordered_set<CppIrId>& active_expressions,
        std::unordered_set<CppIrId>& active_statements
    ) {
        if (!statement_id(id)) {
            return;
        }

        if (!active_statements.insert(id).second) {
            error("cycle in C++ IR statement graph");
            return;
        }

        const auto& statement =
            project_.statements[id];

        const auto verify_optional_expression =
            [&](CppIrId expression) {
                if (expression != invalid_cpp_ir_id) {
                    verify_expression(
                        expression,
                        coroutine,
                        active_expressions,
                        active_statements
                    );
                }
            };

        switch (statement.kind) {
        case CppIrStatementKind::binding:
            if (statement.name.empty()) {
                error(
                    "C++ IR binding has no name"
                );
            }
            if (!statement.type.valid()) {
                error(
                    "C++ IR binding has no target type"
                );
            }
            if (
                statement.expression ==
                invalid_cpp_ir_id
            ) {
                error(
                    "C++ IR binding has no initializer"
                );
            }
            verify_optional_expression(
                statement.expression
            );
            break;

        case CppIrStatementKind::expression:
        case CppIrStatementKind::throw_:
            if (
                statement.expression ==
                invalid_cpp_ir_id
            ) {
                error(
                    "C++ IR statement is missing an expression"
                );
            }
            verify_optional_expression(
                statement.expression
            );
            break;

        case CppIrStatementKind::return_:
            if (coroutine) {
                error(
                    "normal return appears in a C++ IR coroutine"
                );
            }
            verify_optional_expression(
                statement.expression
            );
            break;

        case CppIrStatementKind::co_return:
            if (!coroutine) {
                error(
                    "co_return appears in a non-coroutine C++ IR function"
                );
            }
            verify_optional_expression(
                statement.expression
            );
            break;

        case CppIrStatementKind::block:
            verify_body(
                statement.body,
                coroutine,
                loop_depth,
                active_expressions,
                active_statements
            );
            break;

        case CppIrStatementKind::if_:
            if (
                statement.expression ==
                invalid_cpp_ir_id
            ) {
                error(
                    "C++ IR if statement has no condition"
                );
            }
            verify_optional_expression(
                statement.expression
            );
            verify_body(
                statement.body,
                coroutine,
                loop_depth,
                active_expressions,
                active_statements
            );
            verify_body(
                statement.alternative,
                coroutine,
                loop_depth,
                active_expressions,
                active_statements
            );
            break;

        case CppIrStatementKind::while_:
            if (
                statement.expression ==
                invalid_cpp_ir_id
            ) {
                error(
                    "C++ IR while statement has no condition"
                );
            }
            verify_optional_expression(
                statement.expression
            );
            verify_body(
                statement.body,
                coroutine,
                loop_depth + 1,
                active_expressions,
                active_statements
            );
            break;

        case CppIrStatementKind::for_in:
            if (statement.name.empty()) {
                error(
                    "C++ IR range loop has no binding name"
                );
            }
            if (
                statement.expression ==
                invalid_cpp_ir_id
            ) {
                error(
                    "C++ IR range loop has no range expression"
                );
            }
            verify_optional_expression(
                statement.expression
            );
            verify_body(
                statement.body,
                coroutine,
                loop_depth + 1,
                active_expressions,
                active_statements
            );
            break;

        case CppIrStatementKind::for_:
            if (
                statement.expression ==
                invalid_cpp_ir_id
            ) {
                error(
                    "C++ IR for loop has no condition"
                );
            }
            verify_optional_expression(
                statement.expression
            );

            verify_body(
                statement.parts,
                coroutine,
                loop_depth,
                active_expressions,
                active_statements
            );

            for (auto step : statement.alternative) {
                if (!statement_id(step)) {
                    continue;
                }

                if (
                    project_.statements[step].kind !=
                    CppIrStatementKind::expression
                ) {
                    error(
                        "C++ IR for-loop step is not an expression statement"
                    );
                }

                verify_statement(
                    step,
                    coroutine,
                    loop_depth + 1,
                    active_expressions,
                    active_statements
                );
            }

            verify_body(
                statement.body,
                coroutine,
                loop_depth + 1,
                active_expressions,
                active_statements
            );
            break;

        case CppIrStatementKind::break_:
        case CppIrStatementKind::continue_:
            if (loop_depth == 0) {
                error(
                    "C++ IR loop control appears outside a loop"
                );
            }
            break;
        }

        active_statements.erase(id);
    }

    void verify_body(
        const std::vector<CppIrId>& body,
        bool coroutine,
        std::size_t loop_depth,
        std::unordered_set<CppIrId>& active_expressions,
        std::unordered_set<CppIrId>& active_statements
    ) {
        for (auto statement : body) {
            verify_statement(
                statement,
                coroutine,
                loop_depth,
                active_expressions,
                active_statements
            );
        }
    }

    void verify_arena_shapes() {
        for (
            std::size_t id = 0;
            id < project_.expressions.size();
            ++id
        ) {
            const auto& expression =
                project_.expressions[id];

            if (!expression.type.valid()) {
                error(
                    "C++ IR expression has no target type"
                );
            }

            if (expression.spelling.empty()) {
                error(
                    "C++ IR expression has no target spelling"
                );
            }

            for (auto operand : expression.operands) {
                if (
                    operand == invalid_cpp_ir_id ||
                    operand >= project_.expressions.size()
                ) {
                    error(
                        "C++ IR expression has an invalid operand"
                    );
                }
            }

            if (
                expression.kind ==
                    CppIrExpressionKind::conversion &&
                expression.operands.size() != 1
            ) {
                error(
                    "C++ IR conversion must have one operand"
                );
            }

            if (
                expression.kind ==
                    CppIrExpressionKind::await_ &&
                expression.operands.size() != 1
            ) {
                error(
                    "C++ IR await must have one operand"
                );
            }

            for (auto statement : expression.body) {
                if (
                    statement == invalid_cpp_ir_id ||
                    statement >= project_.statements.size()
                ) {
                    error(
                        "C++ IR expression has an invalid body statement"
                    );
                }
            }

            if (
                expression.kind !=
                    CppIrExpressionKind::lambda &&
                !expression.body.empty()
            ) {
                error(
                    "only lambda C++ IR expressions may own statements"
                );
            }
        }

        for (const auto& statement : project_.statements) {
            const auto check_expression =
                [&](CppIrId expression) {
                    if (
                        expression != invalid_cpp_ir_id &&
                        expression >= project_.expressions.size()
                    ) {
                        error(
                            "C++ IR statement has an invalid expression"
                        );
                    }
                };

            check_expression(statement.expression);

            const auto check_statements =
                [&](const auto& statements) {
                    for (auto child : statements) {
                        if (
                            child == invalid_cpp_ir_id ||
                            child >= project_.statements.size()
                        ) {
                            error(
                                "C++ IR statement has an invalid child statement"
                            );
                        }
                    }
                };

            check_statements(statement.body);
            check_statements(statement.alternative);
            check_statements(statement.parts);
        }
    }

    void verify_functions() {
        for (
            std::size_t function_id = 0;
            function_id < project_.functions.size();
            ++function_id
        ) {
            const auto& function =
                project_.functions[function_id];

            if (function.name.empty()) {
                error(
                    "C++ IR function has no name"
                );
            }

            if (!function.result.valid()) {
                error(
                    "C++ IR function has no result type"
                );
            }

            for (const auto& parameter :
                 function.parameters) {
                if (
                    parameter.name.empty() ||
                    !parameter.type.valid()
                ) {
                    error(
                        "C++ IR function has an invalid parameter"
                    );
                }

                if (
                    parameter.default_value !=
                    invalid_cpp_ir_id
                ) {
                    std::unordered_set<CppIrId>
                        expressions;
                    std::unordered_set<CppIrId>
                        statements;
                    verify_expression(
                        parameter.default_value,
                        function.coroutine,
                        expressions,
                        statements
                    );
                }
            }

            std::unordered_set<CppIrId> expressions;
            std::unordered_set<CppIrId> statements;
            verify_body(
                function.body,
                function.coroutine,
                0,
                expressions,
                statements
            );
        }
    }

    void verify_units() {
        std::vector<std::size_t> membership(
            project_.functions.size(),
            0
        );

        for (const auto& unit : project_.units) {
            for (auto function_id : unit.functions) {
                if (
                    function_id >=
                    project_.functions.size()
                ) {
                    error(
                        "C++ IR unit references an invalid function"
                    );
                    continue;
                }

                ++membership[function_id];

                if (
                    project_.functions[function_id].module !=
                    unit.module
                ) {
                    error(
                        "C++ IR function is assigned to the wrong module unit"
                    );
                }
            }
        }

        for (
            std::size_t function_id = 0;
            function_id < membership.size();
            ++function_id
        ) {
            if (membership[function_id] != 1) {
                error(
                    "C++ IR function must belong to exactly one module unit"
                );
            }
        }
    }

    const CppIrProject& project_;
    std::vector<std::string> errors_;
};

} // namespace

CppIrVerification CppIrVerifier::verify(
    const CppIrProject& project
) const {
    return Verifier{project}.run();
}

} // namespace gungnir::language
