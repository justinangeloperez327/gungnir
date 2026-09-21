#include <gungnir/language/parser.hpp>

#include <utility>

namespace gungnir::language {

Parser::Parser(std::vector<Token> tokens, std::string source_name)
    : tokens_(std::move(tokens)),
      source_name_(std::move(source_name)) {}

std::optional<std::size_t> Parser::next_significant(
    std::size_t index
) const {
    for (auto cursor = index + 1; cursor < tokens_.size(); ++cursor) {
        if (!tokens_[cursor].trivia() && tokens_[cursor].kind != TokenKind::end) {
            return cursor;
        }
    }

    return std::nullopt;
}

std::optional<std::size_t> Parser::previous_significant(
    std::size_t index
) const {
    if (index == 0) {
        return std::nullopt;
    }

    auto cursor = index;
    while (cursor > 0) {
        --cursor;
        if (!tokens_[cursor].trivia() && tokens_[cursor].kind != TokenKind::end) {
            return cursor;
        }
    }

    return std::nullopt;
}

bool Parser::statement_start(std::size_t index) const {
    const auto previous = previous_significant(index);
    if (!previous) {
        return true;
    }

    const auto& lexeme = tokens_[*previous].lexeme;
    return lexeme == "{" || lexeme == "}" || lexeme == ";";
}

bool Parser::declared(const std::string& name) const {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        if (scope->contains(name)) {
            return true;
        }
    }

    return false;
}

bool Parser::declared_here(const std::string& name) const {
    return !scopes_.empty() && scopes_.back().contains(name);
}

void Parser::declare(std::string name) {
    if (scopes_.empty()) {
        scopes_.emplace_back();
    }

    scopes_.back().insert(std::move(name));
}

void Parser::add_duplicate_diagnostic(
    const Token& token,
    const std::string& name
) {
    result_.diagnostics.push_back(Diagnostic{
        DiagnosticLevel::error,
        SourceLocation{source_name_, token.line, token.column},
        "Binding '" + name + "' is already declared in this scope"
    });
}

void Parser::register_explicit_declaration(std::size_t index) {
    if (!statement_start(index)) {
        return;
    }

    const auto& first = tokens_[index];

    if (first.lexeme == "const") {
        const auto type = next_significant(index);
        if (!type || tokens_[*type].kind != TokenKind::identifier) {
            return;
        }

        const auto name = next_significant(*type);
        if (!name || tokens_[*name].kind != TokenKind::identifier) {
            return;
        }

        const auto after = next_significant(*name);
        if (!after) {
            return;
        }

        const auto& marker = tokens_[*after].lexeme;
        if (marker == "=" || marker == ";" || marker == "{") {
            declare(tokens_[*name].lexeme);
        }

        return;
    }

    if (first.kind != TokenKind::identifier) {
        return;
    }

    const auto name = next_significant(index);
    if (!name || tokens_[*name].kind != TokenKind::identifier) {
        return;
    }

    const auto after = next_significant(*name);
    if (!after) {
        return;
    }

    const auto& marker = tokens_[*after].lexeme;
    if (marker == "=" || marker == ";" || marker == "{") {
        declare(tokens_[*name].lexeme);
    }
}

ParseResult Parser::parse() {
    scopes_.clear();
    scopes_.emplace_back();
    result_ = {};

    for (std::size_t index = 0; index < tokens_.size(); ++index) {
        const auto& token = tokens_[index];

        if (token.trivia() || token.kind == TokenKind::end) {
            continue;
        }

        if (token.lexeme == "{") {
            scopes_.emplace_back();
            continue;
        }

        if (token.lexeme == "}") {
            if (scopes_.size() > 1) {
                scopes_.pop_back();
            }
            continue;
        }

        if (token.lexeme == "class") {
            const auto class_name = next_significant(index);
            if (!class_name || tokens_[*class_name].kind != TokenKind::identifier) {
                continue;
            }

            const auto colon = next_significant(*class_name);
            if (!colon || tokens_[*colon].lexeme != ":") {
                continue;
            }

            const auto base = next_significant(*colon);
            if (!base || tokens_[*base].kind != TokenKind::identifier) {
                continue;
            }

            FrameworkBaseKind kind;
            bool framework_base = true;

            if (tokens_[*base].lexeme == "Model") {
                kind = FrameworkBaseKind::model;
            } else if (tokens_[*base].lexeme == "Controller") {
                kind = FrameworkBaseKind::controller;
            } else if (tokens_[*base].lexeme == "Migration") {
                kind = FrameworkBaseKind::migration;
            } else {
                framework_base = false;
            }

            if (framework_base) {
                result_.program.nodes.push_back(FrameworkBase{
                    SourceSpan{
                        tokens_[*base].offset,
                        tokens_[*base].offset + tokens_[*base].lexeme.size(),
                        tokens_[*base].line,
                        tokens_[*base].column
                    },
                    tokens_[*class_name].lexeme,
                    kind
                });
            }

            continue;
        }

        if (token.lexeme == "const" && statement_start(index)) {
            const auto name = next_significant(index);
            if (name && tokens_[*name].kind == TokenKind::identifier) {
                const auto equals = next_significant(*name);

                if (equals && tokens_[*equals].lexeme == "=") {
                    const auto after_equals = next_significant(*equals);
                    const bool comparison =
                        after_equals && tokens_[*after_equals].lexeme == "=";

                    if (!comparison) {
                        const auto binding_name = tokens_[*name].lexeme;

                        if (declared_here(binding_name)) {
                            add_duplicate_diagnostic(tokens_[*name], binding_name);
                        } else {
                            declare(binding_name);
                        }

                        result_.program.nodes.push_back(InferredBinding{
                            SourceSpan{
                                token.offset,
                                tokens_[*name].offset,
                                token.line,
                                token.column
                            },
                            binding_name,
                            true
                        });

                        continue;
                    }
                }
            }
        }

        register_explicit_declaration(index);

        if (
            token.kind == TokenKind::identifier &&
            statement_start(index)
        ) {
            const auto equals = next_significant(index);
            if (!equals || tokens_[*equals].lexeme != "=") {
                continue;
            }

            const auto after_equals = next_significant(*equals);
            if (after_equals && tokens_[*after_equals].lexeme == "=") {
                continue;
            }

            if (declared(token.lexeme)) {
                continue;
            }

            declare(token.lexeme);

            result_.program.nodes.push_back(InferredBinding{
                SourceSpan{
                    token.offset,
                    token.offset,
                    token.line,
                    token.column
                },
                token.lexeme,
                false
            });
        }
    }

    return result_;
}

} // namespace gungnir::language
