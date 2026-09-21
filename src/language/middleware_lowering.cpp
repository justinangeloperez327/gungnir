#include <gungnir/language/middleware_lowering.hpp>

#include <optional>
#include <string_view>
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

} // namespace

MiddlewareLoweringResult MiddlewareLowerer::lower(
    std::string_view source,
    std::string source_name
) const {
    MiddlewareLoweringResult result;

    Lexer lexer{source};
    const auto tokens = lexer.tokenize();

    for (
        std::size_t index = 0;
        index < tokens.size();
        ++index
    ) {
        if (
            tokens[index].trivia() ||
            tokens[index].lexeme != "class"
        ) {
            continue;
        }

        const auto name =
            next_significant(tokens, index);
        const auto colon = name
            ? next_significant(tokens, *name)
            : std::nullopt;
        const auto base = colon
            ? next_significant(tokens, *colon)
            : std::nullopt;

        if (
            !name ||
            !colon ||
            !base ||
            tokens[*name].kind != TokenKind::identifier ||
            tokens[*colon].lexeme != ":" ||
            tokens[*base].lexeme != "Middleware"
        ) {
            continue;
        }

        const auto body_open =
            next_significant(tokens, *base);

        if (
            !body_open ||
            tokens[*body_open].lexeme != "{"
        ) {
            result.diagnostics.push_back(Diagnostic{
                DiagnosticLevel::error,
                SourceLocation{
                    source_name,
                    tokens[*base].line,
                    tokens[*base].column
                },
                "Middleware declaration requires a body"
            });
            continue;
        }

        const auto body_close = matching_symbol(
            tokens,
            *body_open,
            "{",
            "}"
        );

        if (!body_close) {
            result.diagnostics.push_back(Diagnostic{
                DiagnosticLevel::error,
                SourceLocation{
                    source_name,
                    tokens[*body_open].line,
                    tokens[*body_open].column
                },
                "Middleware body is missing a closing brace"
            });
            continue;
        }

        result.edits.push_back(SourceEdit{
            tokens[*body_open].offset + 1,
            tokens[*body_open].offset + 1,
            "\npublic:\n"
        });

        for (
            std::size_t cursor = *body_open + 1;
            cursor < *body_close;
            ++cursor
        ) {
            if (
                tokens[cursor].kind ==
                    TokenKind::identifier &&
                tokens[cursor].lexeme == "Next"
            ) {
                result.edits.push_back(SourceEdit{
                    tokens[cursor].offset,
                    tokens[cursor].offset +
                        tokens[cursor].lexeme.size(),
                    "gungnir::Next"
                });
            }
        }

        const auto after_close =
            next_significant(tokens, *body_close);

        if (
            !after_close ||
            tokens[*after_close].lexeme != ";"
        ) {
            result.edits.push_back(SourceEdit{
                tokens[*body_close].offset + 1,
                tokens[*body_close].offset + 1,
                ";"
            });
        }

        index = *body_close;
    }

    return result;
}

} // namespace gungnir::language
