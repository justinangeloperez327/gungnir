#include <gungnir/language/bootstrap_lowering.hpp>

#include <optional>
#include <vector>

#include <gungnir/language/lexer.hpp>

namespace gungnir::language {

namespace {

std::optional<std::size_t> previous_significant(
    const std::vector<Token>& tokens,
    std::size_t index
) {
    while (index > 0) {
        --index;

        if (
            !tokens[index].trivia() &&
            tokens[index].kind !=
                TokenKind::end
        ) {
            return index;
        }
    }

    return std::nullopt;
}

} // namespace

BootstrapLoweringResult BootstrapLowerer::lower(
    std::string_view source
) const {
    BootstrapLoweringResult result;

    Lexer lexer{source};
    const auto tokens = lexer.tokenize();

    for (
        std::size_t index = 0;
        index < tokens.size();
        ++index
    ) {
        if (
            tokens[index].kind !=
                TokenKind::identifier ||
            tokens[index].lexeme !=
                "Application"
        ) {
            continue;
        }

        const auto previous =
            previous_significant(
                tokens,
                index
            );

        if (
            previous &&
            tokens[*previous].lexeme == ":"
        ) {
            continue;
        }

        result.edits.push_back(SourceEdit{
            tokens[index].offset,
            tokens[index].offset +
                tokens[index].lexeme.size(),
            "gungnir::Application"
        });
    }

    return result;
}

} // namespace gungnir::language
