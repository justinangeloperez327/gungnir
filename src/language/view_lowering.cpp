#include <gungnir/language/view_lowering.hpp>

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
    ViewLoweringResult& result,
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

std::optional<std::size_t> first_top_level_comma(
    const std::vector<Token>& tokens,
    std::size_t open,
    std::size_t close
) {
    std::size_t parentheses = 0;
    std::size_t brackets = 0;
    std::size_t braces = 0;

    for (auto index = open + 1; index < close; ++index) {
        if (tokens[index].trivia()) {
            continue;
        }

        const auto& value = tokens[index].lexeme;

        if (value == "(") {
            ++parentheses;
        } else if (value == ")") {
            if (parentheses > 0) {
                --parentheses;
            }
        } else if (value == "[") {
            ++brackets;
        } else if (value == "]") {
            if (brackets > 0) {
                --brackets;
            }
        } else if (value == "{") {
            ++braces;
        } else if (value == "}") {
            if (braces > 0) {
                --braces;
            }
        } else if (
            value == "," &&
            parentheses == 0 &&
            brackets == 0 &&
            braces == 0
        ) {
            return index;
        }
    }

    return std::nullopt;
}

void lower_object(
    ViewLoweringResult& result,
    const std::vector<Token>& tokens,
    std::size_t open,
    std::size_t close,
    const std::string& source_name
) {
    result.edits.push_back(SourceEdit{
        tokens[open].offset,
        tokens[open].offset + tokens[open].lexeme.size(),
        "gungnir::view::Data{"
    });

    auto cursor = next_significant(tokens, open);

    while (cursor && *cursor < close) {
        const auto key = *cursor;

        if (tokens[key].kind != TokenKind::string_literal) {
            add_error(
                result,
                source_name,
                tokens[key],
                "View data keys must be string literals"
            );
            return;
        }

        const auto colon = next_significant(tokens, key);
        if (
            !colon ||
            *colon >= close ||
            tokens[*colon].lexeme != ":"
        ) {
            add_error(
                result,
                source_name,
                tokens[key],
                "View data entry requires ':' after the key"
            );
            return;
        }

        const auto value_start = next_significant(tokens, *colon);
        if (!value_start || *value_start >= close) {
            add_error(
                result,
                source_name,
                tokens[*colon],
                "View data entry requires a value"
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
            tokens[*colon].offset + tokens[*colon].lexeme.size(),
            ","
        });

        std::size_t parentheses = 0;
        std::size_t brackets = 0;
        std::size_t braces = 0;
        std::optional<std::size_t> separator;

        for (auto index = *value_start; index < close; ++index) {
            if (tokens[index].trivia()) {
                continue;
            }

            const auto& value = tokens[index].lexeme;

            if (value == "(") {
                ++parentheses;
            } else if (value == ")") {
                if (parentheses > 0) {
                    --parentheses;
                }
            } else if (value == "[") {
                ++brackets;
            } else if (value == "]") {
                if (brackets > 0) {
                    --brackets;
                }
            } else if (value == "{") {
                ++braces;
            } else if (value == "}") {
                if (braces > 0) {
                    --braces;
                }
            } else if (
                value == "," &&
                parentheses == 0 &&
                brackets == 0 &&
                braces == 0
            ) {
                separator = index;
                break;
            }
        }

        const auto end_offset = separator
            ? tokens[*separator].offset
            : tokens[close].offset;

        result.edits.push_back(SourceEdit{
            end_offset,
            end_offset,
            "}"
        });

        if (!separator) {
            break;
        }

        cursor = next_significant(tokens, *separator);
    }
}

} // namespace

ViewLoweringResult ViewLowerer::lower(
    std::string_view source,
    std::string source_name
) const {
    ViewLoweringResult result;

    Lexer lexer{source};
    const auto tokens = lexer.tokenize();

    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (
            tokens[index].kind != TokenKind::identifier ||
            tokens[index].lexeme != "view"
        ) {
            continue;
        }

        const auto open = next_significant(tokens, index);
        if (!open || tokens[*open].lexeme != "(") {
            continue;
        }

        const auto close = matching_symbol(tokens, *open, "(", ")");
        if (!close) {
            add_error(
                result,
                source_name,
                tokens[*open],
                "view(...) is missing a closing parenthesis"
            );
            continue;
        }

        const auto comma = first_top_level_comma(
            tokens,
            *open,
            *close
        );

        if (!comma) {
            index = *close;
            continue;
        }

        const auto data_open = next_significant(tokens, *comma);
        if (
            !data_open ||
            *data_open >= *close ||
            tokens[*data_open].lexeme != "{"
        ) {
            index = *close;
            continue;
        }

        const auto data_close = matching_symbol(
            tokens,
            *data_open,
            "{",
            "}"
        );

        if (!data_close || *data_close > *close) {
            add_error(
                result,
                source_name,
                tokens[*data_open],
                "View data object is missing a closing brace"
            );
            continue;
        }

        lower_object(
            result,
            tokens,
            *data_open,
            *data_close,
            source_name
        );

        index = *close;
    }

    return result;
}

} // namespace gungnir::language
