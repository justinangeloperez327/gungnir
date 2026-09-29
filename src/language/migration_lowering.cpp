#include <gungnir/language/migration_lowering.hpp>

#include <optional>
#include <string_view>
#include <vector>

#include <gungnir/language/lexer.hpp>
#include <gungnir/language/parser.hpp>

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


} // namespace

MigrationLoweringResult
MigrationLowerer::lower(
    std::string_view source,
    std::string source_name
) const {
    Parser parser{Lexer{source}.tokenize(), source_name};
    return lower(source, parser.parse().program, std::move(source_name));
}

MigrationLoweringResult MigrationLowerer::lower(
    std::string_view source, const Program& program,
    std::string source_name
) const {
    return lower(source, program, Lexer{source}.tokenize(), std::move(source_name));
}

MigrationLoweringResult MigrationLowerer::lower(
    std::string_view source, const Program& program,
    const std::vector<Token>& tokens, std::string source_name
) const {
    MigrationLoweringResult result;

    for (const auto& node : program.nodes) {
        const auto* base = std::get_if<FrameworkBase>(&node);
        const auto* framework = std::get_if<FrameworkDeclaration>(&node);
        if ((!base || base->kind != FrameworkBaseKind::migration) &&
            (!framework || framework->kind != FrameworkBaseKind::migration)) {
            continue;
        }
        const auto span = base ? base->declaration_span : framework->span;
        const auto keyword = base ? base->span : framework->keyword_span;
        const auto body = base ? base->body_open_span : framework->body_open_span;
        const auto body_open = body.begin;
        if (body.end == 0 || body_open >= span.end || span.end == 0) {
            result.diagnostics.push_back(Diagnostic{
                DiagnosticLevel::error,
                SourceLocation{source_name, keyword.line, keyword.column},
                "Migration declaration requires a body"
            });
            continue;
        }
        const auto body_close = span.end - 1;
        result.edits.push_back(SourceEdit{body_open + 1, body_open + 1,
                                          "\npublic:\n"});
        for (std::size_t index = 0; index < tokens.size(); ++index) {
            if (tokens[index].offset != body_close) continue;
            const auto next = next_significant(tokens, index);
            if (!next || tokens[*next].lexeme != ";") {
                result.edits.push_back(SourceEdit{body_close + 1,
                                                   body_close + 1, ";"});
            }
            break;
        }
    }

    return result;
}

} // namespace gungnir::language
