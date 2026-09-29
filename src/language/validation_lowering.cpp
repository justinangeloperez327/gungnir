#include <gungnir/language/validation_lowering.hpp>

#include <optional>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gungnir/language/lexer.hpp>

namespace gungnir::language {

namespace {

std::optional<std::size_t> next_significant(
    const std::vector<Token>& tokens,
    std::size_t index
) {
    for (
        auto cursor = index + 1;
        cursor < tokens.size();
        ++cursor
    ) {
        if (
            !tokens[cursor].trivia() &&
            tokens[cursor].kind != TokenKind::end
        ) {
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

    for (
        auto index = opening;
        index < tokens.size();
        ++index
    ) {
        if (tokens[index].trivia()) {
            continue;
        }

        if (tokens[index].lexeme == open) {
            ++depth;
        } else if (tokens[index].lexeme == close) {
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
    ValidationLoweringResult& result,
    const std::string& file,
    const Token& token,
    std::string message
) {
    result.diagnostics.push_back(Diagnostic{
        DiagnosticLevel::error,
        SourceLocation{
            file,
            token.line,
            token.column
        },
        std::move(message)
    });
}

void lower_rules(
    ValidationLoweringResult& result,
    const std::vector<Token>& tokens,
    std::size_t open,
    std::size_t close,
    const std::string& source_name
) {
    result.edits.push_back(SourceEdit{
        tokens[open].offset,
        tokens[open].offset + 1,
        "gungnir::validation::Rules{"
    });

    auto cursor = next_significant(tokens, open);

    while (cursor && *cursor < close) {
        const auto key = *cursor;

        if (
            tokens[key].kind !=
            TokenKind::string_literal
        ) {
            add_error(
                result,
                source_name,
                tokens[key],
                "Validation field names must be string literals"
            );
            return;
        }

        const auto colon =
            next_significant(tokens, key);

        if (
            !colon ||
            *colon >= close ||
            tokens[*colon].lexeme != ":"
        ) {
            add_error(
                result,
                source_name,
                tokens[key],
                "Validation rule requires ':' after the field name"
            );
            return;
        }

        const auto value =
            next_significant(tokens, *colon);

        if (
            !value ||
            *value >= close ||
            tokens[*value].kind !=
                TokenKind::string_literal
        ) {
            add_error(
                result,
                source_name,
                tokens[*colon],
                "Validation rules must be string literals"
            );
            return;
        }

        result.edits.push_back(SourceEdit{
            tokens[key].offset,
            tokens[key].offset,
            "{"
        });

        result.edits.push_back(SourceEdit{
            tokens[*colon].offset,
            tokens[*colon].offset + 1,
            ","
        });

        const auto after_value =
            next_significant(tokens, *value);

        if (
            after_value &&
            *after_value < close &&
            tokens[*after_value].lexeme == ","
        ) {
            result.edits.push_back(SourceEdit{
                tokens[*after_value].offset,
                tokens[*after_value].offset,
                "}"
            });

            cursor = next_significant(
                tokens,
                *after_value
            );
            continue;
        }

        result.edits.push_back(SourceEdit{
            tokens[*value].offset +
                tokens[*value].lexeme.size(),
            tokens[*value].offset +
                tokens[*value].lexeme.size(),
            "}"
        });

        break;
    }
}

} // namespace

ValidationLoweringResult ValidationLowerer::lower(
    std::string_view source,
    std::string source_name
) const {
    return lower(Lexer{source}.tokenize(), std::move(source_name));
}

ValidationLoweringResult ValidationLowerer::lower(
    const std::vector<Token>& tokens,
    std::string source_name
) const {
    return lower({}, tokens, Program{}, std::move(source_name));
}

ValidationLoweringResult ValidationLowerer::lower(
    std::string_view source, const std::vector<Token>& tokens,
    const Program& program, std::string source_name
) const {
    ValidationLoweringResult result;
    (void) source;
    std::unordered_set<std::size_t> handled_calls;
    const auto symbol_between = [&](std::size_t begin, std::size_t end,
                                    std::string_view symbol) -> std::optional<std::size_t> {
        for (const auto& token : tokens) {
            if (token.offset >= end) break;
            if (token.offset >= begin && token.lexeme == symbol)
                return token.offset;
        }
        return std::nullopt;
    };
    std::function<void(const Expression&)> lower_expression =
        [&](const Expression& expression) {
        if (expression.kind == ExpressionKind::call &&
            expression.arguments.size() >= 2 &&
            expression.arguments[1].kind == ExpressionKind::object) {
            const auto& callee = expression.arguments.front();
            const Expression* name = &callee;
            if (callee.kind == ExpressionKind::member &&
                callee.arguments.size() == 2) name = &callee.arguments[1];
            if (name->kind == ExpressionKind::name && name->text == "validate") {
                const auto& object = expression.arguments[1];
                handled_calls.insert(name->span.begin);
                result.edits.push_back(SourceEdit{
                    object.span.begin, object.span.begin + 1,
                    "gungnir::validation::Rules{"
                });
                for (std::size_t i = 0; i < object.arguments.size(); ++i) {
                    const auto& entry = object.arguments[i];
                    if (entry.kind != ExpressionKind::entry ||
                        entry.arguments.size() != 2) continue;
                    const auto& key = entry.arguments[0];
                    const auto& value = entry.arguments[1];
                    const auto diagnostic = [&](const Expression& at,
                                                std::string message) {
                        result.diagnostics.push_back(Diagnostic{
                            DiagnosticLevel::error,
                            SourceLocation{source_name, at.span.line, at.span.column},
                            std::move(message)
                        });
                    };
                    if (key.kind != ExpressionKind::literal ||
                        key.text.empty() || key.text.front() != '"') {
                        diagnostic(key, "Validation field names must be string literals");
                        continue;
                    }
                    if (value.kind != ExpressionKind::literal ||
                        value.text.empty() || value.text.front() != '"') {
                        diagnostic(value, "Validation rules must be string literals");
                        continue;
                    }
                    const auto colon = symbol_between(key.span.end, value.span.begin, ":");
                    if (!colon) continue;
                    result.edits.push_back(SourceEdit{key.span.begin, key.span.begin, "{"});
                    result.edits.push_back(SourceEdit{*colon, *colon + 1, ","});
                    auto end = value.span.end;
                    if (i + 1 < object.arguments.size()) {
                        const auto comma = symbol_between(
                            value.span.end, object.arguments[i + 1].span.begin, ",");
                        if (comma) end = *comma;
                    }
                    result.edits.push_back(SourceEdit{end, end, "}"});
                }
            }
        }
        for (const auto& argument : expression.arguments) lower_expression(argument);
    };
    std::function<void(const std::vector<MethodStatement>&)> lower_statements =
        [&](const std::vector<MethodStatement>& statements) {
        for (const auto& statement : statements) {
            lower_expression(statement.expression);
            lower_statements(statement.children);
            lower_statements(statement.alternative);
        }
    };
    for (const auto& node : program.nodes) {
        if (const auto* method = std::get_if<FrameworkMethod>(&node))
            lower_statements(method->body);
        if (const auto* method = std::get_if<ControllerMethod>(&node))
            lower_statements(method->body);
    }

    for (
        std::size_t index = 0;
        index < tokens.size();
        ++index
    ) {
        if (
            tokens[index].kind != TokenKind::identifier ||
            tokens[index].lexeme != "validate" ||
            handled_calls.contains(tokens[index].offset)
        ) {
            continue;
        }

        const auto open =
            next_significant(tokens, index);

        if (!open || tokens[*open].lexeme != "(") {
            continue;
        }

        const auto close = matching_symbol(
            tokens,
            *open,
            "(",
            ")"
        );

        if (!close) {
            add_error(
                result,
                source_name,
                tokens[*open],
                "validate(...) is missing a closing parenthesis"
            );
            continue;
        }

        const auto rules_open =
            next_significant(tokens, *open);

        if (
            !rules_open ||
            *rules_open >= *close ||
            tokens[*rules_open].lexeme != "{"
        ) {
            index = *close;
            continue;
        }

        const auto rules_close = matching_symbol(
            tokens,
            *rules_open,
            "{",
            "}"
        );

        if (
            !rules_close ||
            *rules_close > *close
        ) {
            add_error(
                result,
                source_name,
                tokens[*rules_open],
                "Validation rules object is missing a closing brace"
            );
            continue;
        }

        lower_rules(
            result,
            tokens,
            *rules_open,
            *rules_close,
            source_name
        );

        index = *close;
    }

    return result;
}

} // namespace gungnir::language
