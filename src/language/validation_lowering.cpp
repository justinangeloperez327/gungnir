#include <gungnir/language/validation_lowering.hpp>

#include <optional>
#include <string>
#include <string_view>
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
    ValidationLoweringResult result;

    Lexer lexer{source};
    const auto tokens = lexer.tokenize();

    for (
        std::size_t index = 0;
        index < tokens.size();
        ++index
    ) {
        if (
            tokens[index].kind != TokenKind::identifier ||
            tokens[index].lexeme != "validate"
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
