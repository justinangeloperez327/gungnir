#include <gungnir/language/async_lowering.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <string_view>
#include <utility>
#include <vector>

#include <gungnir/language/lexer.hpp>

namespace gungnir::language {

namespace {

struct AsyncFunction {
    std::size_t async_index{0};
    std::size_t return_type_index{0};
    std::size_t name_index{0};
    std::size_t parameters_open{0};
    std::size_t parameters_close{0};
    std::size_t body_open{0};
    std::size_t body_close{0};
};

std::optional<std::size_t> next_significant(
    const std::vector<Token>& tokens,
    std::size_t index
) {
    for (auto cursor = index + 1; cursor < tokens.size(); ++cursor) {
        if (!tokens[cursor].trivia() && tokens[cursor].kind != TokenKind::end) {
            return cursor;
        }
    }

    return std::nullopt;
}

std::optional<std::size_t> matching_symbol(
    const std::vector<Token>& tokens,
    std::size_t opening,
    std::string_view open,
    std::string_view close
) {
    std::size_t depth = 0;

    for (auto index = opening; index < tokens.size(); ++index) {
        if (tokens[index].trivia()) {
            continue;
        }

        if (tokens[index].lexeme == open) {
            ++depth;
            continue;
        }

        if (tokens[index].lexeme == close) {
            if (depth == 0) {
                return std::nullopt;
            }

            --depth;
            if (depth == 0) {
                return index;
            }
        }
    }

    return std::nullopt;
}

void add_error(
    AsyncLoweringResult& result,
    const std::string& file,
    const Token& token,
    std::string message
) {
    result.diagnostics.push_back(Diagnostic{
        DiagnosticLevel::error,
        SourceLocation{file, token.line, token.column},
        std::move(message)
    });
}

std::string qualify_framework_type(std::string_view type) {
    if (type == "Response") {
        return "gungnir::Response";
    }

    if (type == "Request") {
        return "gungnir::Request";
    }

    return std::string{type};
}

bool inside(
    std::size_t index,
    const AsyncFunction& function
) {
    return index > function.body_open && index < function.body_close;
}

std::optional<AsyncFunction> containing_async_function(
    std::size_t index,
    const std::vector<AsyncFunction>& functions
) {
    for (const auto& function : functions) {
        if (inside(index, function)) {
            return function;
        }
    }

    return std::nullopt;
}

void lower_request_parameters(
    AsyncLoweringResult& result,
    const std::vector<Token>& tokens,
    std::size_t open,
    std::size_t close
) {
    for (auto index = open + 1; index < close; ++index) {
        if (
            tokens[index].kind != TokenKind::identifier ||
            tokens[index].lexeme != "Request"
        ) {
            continue;
        }

        const auto next = next_significant(tokens, index);
        if (!next || *next >= close) {
            continue;
        }

        if (tokens[*next].lexeme == "&") {
            result.edits.push_back(SourceEdit{
                tokens[index].offset,
                tokens[index].offset + tokens[index].lexeme.size(),
                "gungnir::Request"
            });
            continue;
        }

        if (tokens[*next].kind == TokenKind::identifier) {
            result.edits.push_back(SourceEdit{
                tokens[index].offset,
                tokens[index].offset + tokens[index].lexeme.size(),
                "gungnir::Request&"
            });
        }
    }
}

} // namespace

AsyncLoweringResult AsyncLowerer::lower(
    std::string_view source,
    std::string source_name
) const {
    return lower(Lexer{source}.tokenize(), std::move(source_name));
}

AsyncLoweringResult AsyncLowerer::lower(
    const std::vector<Token>& tokens,
    std::string source_name
) const {
    return lower(tokens, Program{}, std::move(source_name));
}

AsyncLoweringResult AsyncLowerer::lower(
    const std::vector<Token>& tokens,
    const Program& program,
    std::string source_name
) const {
    AsyncLoweringResult result;

    std::vector<AsyncFunction> functions;
    std::unordered_set<std::size_t> lowered_words;
    std::unordered_map<std::size_t, std::size_t> token_indices;
    token_indices.reserve(tokens.size());
    for (std::size_t index = 0; index < tokens.size(); ++index)
        token_indices.emplace(tokens[index].offset, index);
    const auto at_offset = [&](std::size_t offset) -> std::optional<std::size_t> {
        const auto found = token_indices.find(offset);
        return found == token_indices.end()
            ? std::nullopt : std::optional<std::size_t>{found->second};
    };
    std::function<void(const Expression&)> lower_expression =
        [&](const Expression& expression) {
        if (expression.kind == ExpressionKind::unary &&
            expression.text == "await" &&
            lowered_words.insert(expression.span.begin).second) {
            result.edits.push_back(SourceEdit{
                expression.span.begin, expression.span.begin + 5, "co_await"
            });
        }
        for (const auto& child : expression.arguments) lower_expression(child);
    };
    std::function<void(const std::vector<MethodStatement>&)> lower_body =
        [&](const std::vector<MethodStatement>& statements) {
        for (const auto& statement : statements) {
            if (statement.kind == StatementKind::return_ &&
                lowered_words.insert(statement.span.begin).second) {
                result.edits.push_back(SourceEdit{
                    statement.span.begin, statement.span.begin + 6, "co_return"
                });
            }
            lower_expression(statement.expression);
            lower_body(statement.children);
            lower_body(statement.alternative);
        }
    };
    const auto lower_method = [&](const auto& method) {
        if (!method.asynchronous) return;
        const auto start = at_offset(method.span.begin);
        const auto return_type = at_offset(method.return_type_span.begin);
        const auto name = at_offset(method.name_span.begin);
        if (!start || !return_type || !name ||
            tokens[*start].lexeme != "async") return;
        const auto open = next_significant(tokens, *name);
        const auto close = open ? matching_symbol(tokens, *open, "(", ")")
                                : std::nullopt;
        const auto body_open = close ? next_significant(tokens, *close)
                                     : std::nullopt;
        const auto body_close = body_open
            ? matching_symbol(tokens, *body_open, "{", "}") : std::nullopt;
        if (!open || !close || !body_open || !body_close) return;
        functions.push_back(AsyncFunction{
            *start, *return_type, *name, *open, *close, *body_open, *body_close
        });
        lowered_words.insert(method.span.begin);
        result.edits.push_back(SourceEdit{
            method.span.begin, method.return_type_span.end,
            "gungnir::Task<" + qualify_framework_type(method.return_type) + ">"
        });
        lower_request_parameters(result, tokens, *open, *close);
        lower_body(method.body);
    };
    for (const auto& node : program.nodes) {
        if (const auto* method = std::get_if<FrameworkMethod>(&node))
            lower_method(*method);
        if (const auto* method = std::get_if<ControllerMethod>(&node))
            lower_method(*method);
    }

    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (
            !tokens[index].word() ||
            tokens[index].lexeme != "async" ||
            lowered_words.contains(tokens[index].offset)
        ) {
            continue;
        }

        const auto return_type = next_significant(tokens, index);
        const auto name = return_type
            ? next_significant(tokens, *return_type)
            : std::nullopt;
        const auto parameters_open = name
            ? next_significant(tokens, *name)
            : std::nullopt;

        if (
            !return_type ||
            !name ||
            !parameters_open ||
            tokens[*return_type].kind != TokenKind::identifier ||
            tokens[*name].kind != TokenKind::identifier ||
            tokens[*parameters_open].lexeme != "("
        ) {
            add_error(
                result,
                source_name,
                tokens[index],
                "async requires: async ReturnType function(...)"
            );
            continue;
        }

        const auto parameters_close = matching_symbol(
            tokens,
            *parameters_open,
            "(",
            ")"
        );

        if (!parameters_close) {
            add_error(
                result,
                source_name,
                tokens[*parameters_open],
                "Async function parameters are missing a closing parenthesis"
            );
            continue;
        }

        const auto body_open = next_significant(tokens, *parameters_close);
        if (!body_open || tokens[*body_open].lexeme != "{") {
            add_error(
                result,
                source_name,
                tokens[*name],
                "Async function requires a body"
            );
            continue;
        }

        const auto body_close = matching_symbol(
            tokens,
            *body_open,
            "{",
            "}"
        );

        if (!body_close) {
            add_error(
                result,
                source_name,
                tokens[*body_open],
                "Async function body is missing a closing brace"
            );
            continue;
        }

        const AsyncFunction function{
            index,
            *return_type,
            *name,
            *parameters_open,
            *parameters_close,
            *body_open,
            *body_close
        };

        functions.push_back(function);

        const auto native_return = qualify_framework_type(
            tokens[*return_type].lexeme
        );

        result.edits.push_back(SourceEdit{
            tokens[index].offset,
            tokens[*return_type].offset +
                tokens[*return_type].lexeme.size(),
            "gungnir::Task<" + native_return + ">"
        });

        lower_request_parameters(
            result,
            tokens,
            *parameters_open,
            *parameters_close
        );

        for (
            auto cursor = *body_open + 1;
            cursor < *body_close;
            ++cursor
        ) {
            if (!tokens[cursor].word()) {
                continue;
            }

            if (tokens[cursor].lexeme == "await") {
                if (!lowered_words.insert(tokens[cursor].offset).second) continue;
                result.edits.push_back(SourceEdit{
                    tokens[cursor].offset,
                    tokens[cursor].offset + tokens[cursor].lexeme.size(),
                    "co_await"
                });
            } else if (tokens[cursor].lexeme == "return") {
                if (!lowered_words.insert(tokens[cursor].offset).second) continue;
                result.edits.push_back(SourceEdit{
                    tokens[cursor].offset,
                    tokens[cursor].offset + tokens[cursor].lexeme.size(),
                    "co_return"
                });
            }
        }

        index = *body_close;
    }

    // Preserve native C++ escape expressions inside AST-recognized methods.
    for (const auto& function : functions) {
        for (auto cursor = function.body_open + 1;
             cursor < function.body_close; ++cursor) {
            if (!tokens[cursor].word() ||
                !lowered_words.insert(tokens[cursor].offset).second) continue;
            if (tokens[cursor].lexeme == "await" ||
                tokens[cursor].lexeme == "return") {
                result.edits.push_back(SourceEdit{
                    tokens[cursor].offset,
                    tokens[cursor].offset + tokens[cursor].lexeme.size(),
                    tokens[cursor].lexeme == "await" ? "co_await" : "co_return"
                });
            }
        }
    }

    // Clean framework types from normal controller-style signatures too.
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (
            tokens[index].kind != TokenKind::identifier ||
            (
                tokens[index].lexeme != "Response" &&
                tokens[index].lexeme != "Request"
            )
        ) {
            continue;
        }

        bool already_lowered = false;
        for (const auto& function : functions) {
            if (
                index == function.return_type_index ||
                (
                    index > function.parameters_open &&
                    index < function.parameters_close
                )
            ) {
                already_lowered = true;
                break;
            }
        }

        if (already_lowered) {
            continue;
        }

        const auto next = next_significant(tokens, index);
        if (!next) {
            continue;
        }

        if (
            tokens[index].lexeme == "Response" &&
            tokens[*next].kind == TokenKind::identifier
        ) {
            const auto after_name = next_significant(tokens, *next);
            if (after_name && tokens[*after_name].lexeme == "(") {
                result.edits.push_back(SourceEdit{
                    tokens[index].offset,
                    tokens[index].offset + tokens[index].lexeme.size(),
                    "gungnir::Response"
                });

                const auto close = matching_symbol(
                    tokens,
                    *after_name,
                    "(",
                    ")"
                );

                if (close) {
                    lower_request_parameters(
                        result,
                        tokens,
                        *after_name,
                        *close
                    );
                }
            }
        }
    }

    // await outside an async function is a language error.
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (
            tokens[index].word() &&
            tokens[index].lexeme == "await" &&
            !containing_async_function(index, functions)
        ) {
            add_error(
                result,
                source_name,
                tokens[index],
                "await can only be used inside an async function"
            );
        }
    }

    return result;
}

} // namespace gungnir::language
