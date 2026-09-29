#include <gungnir/language/parser.hpp>

#include <optional>
#include <functional>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace gungnir::language {

namespace {

std::optional<FrameworkBaseKind> framework_kind(
    const std::string& lexeme
) {
    if (lexeme == "model") {
        return FrameworkBaseKind::model;
    }

    if (lexeme == "controller") {
        return FrameworkBaseKind::controller;
    }

    if (lexeme == "migration") {
        return FrameworkBaseKind::migration;
    }

    if (lexeme == "middleware") {
        return FrameworkBaseKind::middleware;
    }

    if (lexeme == "policy") {
        return FrameworkBaseKind::policy;
    }

    if (lexeme == "event") {
        return FrameworkBaseKind::event;
    }

    if (lexeme == "listener") {
        return FrameworkBaseKind::listener;
    }

    if (lexeme == "notification") {
        return FrameworkBaseKind::notification;
    }

    if (lexeme == "mail") {
        return FrameworkBaseKind::mail;
    }

    return std::nullopt;
}

std::optional<ModelConfigurationKind> model_configuration_kind(
    std::string_view lexeme
) {
    if (lexeme == "table") {
        return ModelConfigurationKind::table;
    }

    if (lexeme == "connection") {
        return ModelConfigurationKind::connection;
    }

    if (lexeme == "timestamps") {
        return ModelConfigurationKind::timestamps;
    }

    if (lexeme == "softDeletes") {
        return ModelConfigurationKind::soft_deletes;
    }

    return std::nullopt;
}

std::optional<RouteMethodKind> route_method_kind(
    std::string_view lexeme
) {
    if (lexeme == "get") {
        return RouteMethodKind::get;
    }

    if (lexeme == "post") {
        return RouteMethodKind::post;
    }

    if (lexeme == "put") {
        return RouteMethodKind::put;
    }

    if (lexeme == "patch") {
        return RouteMethodKind::patch;
    }

    if (lexeme == "delete" || lexeme == "remove") {
        return RouteMethodKind::remove;
    }

    if (lexeme == "options") {
        return RouteMethodKind::options;
    }

    if (lexeme == "head") {
        return RouteMethodKind::head;
    }

    return std::nullopt;
}

std::optional<ModelRelationshipKind> model_relationship_kind(
    std::string_view lexeme
) {
    if (lexeme == "hasOne") {
        return ModelRelationshipKind::has_one;
    }

    if (lexeme == "hasMany") {
        return ModelRelationshipKind::has_many;
    }

    if (lexeme == "belongsTo") {
        return ModelRelationshipKind::belongs_to;
    }

    if (lexeme == "belongsToMany") {
        return ModelRelationshipKind::belongs_to_many;
    }

    if (lexeme == "hasOneThrough") {
        return ModelRelationshipKind::has_one_through;
    }

    if (lexeme == "hasManyThrough") {
        return ModelRelationshipKind::has_many_through;
    }

    return std::nullopt;
}

bool relationship_requires_through(ModelRelationshipKind kind) {
    return
        kind == ModelRelationshipKind::has_one_through ||
        kind == ModelRelationshipKind::has_many_through;
}

bool scalar_type(std::string_view lexeme) {
    return
        lexeme == "string" ||
        lexeme == "int" ||
        lexeme == "integer" ||
        lexeme == "int64" ||
        lexeme == "uint64" ||
        lexeme == "bool" ||
        lexeme == "boolean" ||
        lexeme == "float" ||
        lexeme == "double";
}

std::string unquote(std::string_view value) {
    if (
        value.size() >= 2 &&
        (
            (value.front() == '"' && value.back() == '"') ||
            (value.front() == '\'' && value.back() == '\'')
        )
    ) {
        return std::string{value.substr(1, value.size() - 2)};
    }

    return std::string{value};
}

SourceSpan token_span(const Token& token) {
    return SourceSpan{
        token.offset,
        token.offset + token.lexeme.size(),
        token.line,
        token.column
    };
}

std::string declaration_name(
    FrameworkBaseKind kind
) {
    switch (kind) {
    case FrameworkBaseKind::model:
        return "Model";
    case FrameworkBaseKind::controller:
        return "Controller";
    case FrameworkBaseKind::migration:
        return "Migration";
    case FrameworkBaseKind::middleware:
        return "Middleware";
    case FrameworkBaseKind::policy:
        return "Policy";
    case FrameworkBaseKind::event:
        return "Event";
    case FrameworkBaseKind::listener:
        return "Listener";
    case FrameworkBaseKind::notification:
        return "Notification";
    case FrameworkBaseKind::mail:
        return "Mail";
    }

    return "Framework";
}

} // namespace

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

std::optional<std::size_t> Parser::matching_symbol(
    std::size_t opening,
    std::string_view open,
    std::string_view close
) const {
    std::size_t depth = 0;

    for (auto index = opening; index < tokens_.size(); ++index) {
        if (tokens_[index].trivia()) {
            continue;
        }

        if (tokens_[index].lexeme == open) {
            ++depth;
            continue;
        }

        if (tokens_[index].lexeme == close) {
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

std::vector<MethodParameter> Parser::parse_parameters(
    std::size_t opening, std::size_t closing
) const {
    std::vector<MethodParameter> parameters;
    auto first = next_significant(opening);
    while (first && *first < closing) {
        auto last = *first;
        auto cursor = next_significant(last);
        std::size_t nested = 0;
        while (cursor && *cursor < closing) {
            const auto& value = tokens_[*cursor].lexeme;
            if (value == "<" || value == "(" || value == "[") {
                ++nested;
            } else if (value == ">" || value == ")" || value == "]") {
                if (nested > 0) --nested;
            }
            if (value == "," && nested == 0) break;
            last = *cursor;
            cursor = next_significant(last);
        }
        if (*first != last && tokens_[last].kind == TokenKind::identifier) {
            parameters.push_back(MethodParameter{
                SourceSpan{tokens_[*first].offset,
                           tokens_[last].offset + tokens_[last].lexeme.size(),
                           tokens_[*first].line, tokens_[*first].column},
                tokens_[*first].lexeme, tokens_[last].lexeme
            });
        }
        first = cursor ? next_significant(*cursor) : std::nullopt;
    }
    return parameters;
}

Expression Parser::parse_expression(std::size_t first, std::size_t last) {
    std::vector<std::size_t> significant;
    for (auto i = first; i <= last; ++i) {
        if (!tokens_[i].trivia()) significant.push_back(i);
    }
    const auto value = [&](std::size_t i) -> const std::string& {
        return tokens_[significant[i]].lexeme;
    };
    const auto error = [&](std::size_t i, std::string message,
                           std::string code = "GNR1010") {
        const auto& token = tokens_[significant[i]];
        result_.diagnostics.push_back(Diagnostic{
            DiagnosticLevel::error,
            SourceLocation{source_name_, token.line, token.column},
            std::move(message), std::move(code), {}
        });
    };
    const auto make = [&](std::size_t begin, std::size_t end,
                          ExpressionKind kind, std::string text = {}) {
        const auto& start = tokens_[significant[begin]];
        const auto& finish = tokens_[significant[end - 1]];
        return Expression{SourceSpan{start.offset,
                          finish.offset + finish.lexeme.size(),
                          start.line, start.column}, kind, std::move(text), {}};
    };
    const auto precedence = [](std::string_view op) {
        if (op == "=") return 1;
        if (op == "||") return 2;
        if (op == "&&") return 3;
        if (op == "==" || op == "!=") return 4;
        if (op == "<" || op == ">" || op == "<=" || op == ">=") return 5;
        if (op == "+" || op == "-") return 6;
        if (op == "*" || op == "/" || op == "%") return 7;
        return 0;
    };
    std::vector<std::string_view> delimiters;
    for (std::size_t i = 0; i < significant.size(); ++i) {
        const auto& symbol = value(i);
        if (symbol == "(" || symbol == "[" || symbol == "{") {
            delimiters.push_back(symbol);
        } else if (symbol == ")" || symbol == "]" || symbol == "}") {
            const bool matches = !delimiters.empty() &&
                ((delimiters.back() == "(" && symbol == ")") ||
                 (delimiters.back() == "[" && symbol == "]") ||
                 (delimiters.back() == "{" && symbol == "}"));
            if (!matches) {
                error(i, "Unexpected closing delimiter", "GNR1011");
                break;
            }
            delimiters.pop_back();
        }
    }
    if (!delimiters.empty())
        error(0, "Expression has an unclosed delimiter", "GNR1011");
    if (significant.size() > 1) {
        const auto last = significant.size() - 1;
        const auto& tail = value(last);
        const bool postfix = last > 0 &&
            tokens_[significant[last - 1]].offset +
                tokens_[significant[last - 1]].lexeme.size() ==
                tokens_[significant[last]].offset &&
            ((tail == "+" && value(last - 1) == "+") ||
             (tail == "-" && value(last - 1) == "-"));
        if (!postfix && (tail == "=" || tail == "+" || tail == "-" ||
                         tail == "*" || tail == "/" || tail == "%" ||
                         tail == "&" || tail == "|" || tail == "!"))
            error(last, "Operator is missing its right operand", "GNR1012");
    }
    std::function<Expression(std::size_t, std::size_t)> parse =
        [&](std::size_t begin, std::size_t end) -> Expression {
        auto raw = make(begin, end, ExpressionKind::raw, value(begin));
        if (begin + 1 == end) {
            const auto& token = tokens_[significant[begin]];
            if (!token.word() && token.kind != TokenKind::string_literal &&
                token.kind != TokenKind::number &&
                token.kind != TokenKind::character_literal)
                error(begin, "Expected an expression");
            raw.kind = token.kind == TokenKind::string_literal ||
                       token.kind == TokenKind::number || value(begin) == "true" ||
                       value(begin) == "false" || value(begin) == "null"
                       ? ExpressionKind::literal : token.word()
                       ? ExpressionKind::name : ExpressionKind::raw;
            return raw;
        }
        // Split at the weakest operator outside nested delimiters.
        std::size_t split = end;
        std::size_t split_width = 1;
        std::string split_operator;
        int weakest = 8;
        int parens = 0, brackets = 0, braces = 0;
        for (auto i = begin; i < end; ++i) {
            std::string op = value(i);
            if (op == "(") ++parens;
            else if (op == ")") --parens;
            else if (op == "[") ++brackets;
            else if (op == "]") --brackets;
            else if (op == "{") ++braces;
            else if (op == "}") --braces;
            if (parens || brackets || braces || i == begin || i + 1 == end)
                continue;
            std::size_t width = 1;
            if (i + 1 < end &&
                tokens_[significant[i]].offset + tokens_[significant[i]].lexeme.size() ==
                    tokens_[significant[i + 1]].offset) {
                const auto combined = op + value(i + 1);
                if (combined == "&&" || combined == "||" || combined == "==" ||
                    combined == "!=" || combined == "<=" || combined == ">=" ||
                    combined == "->" || combined == "::" ||
                    combined == "++" || combined == "--") {
                    op = combined;
                    width = 2;
                }
            }
            const int rank = precedence(op);
            if (rank && i + width < end &&
                (rank < weakest || (rank == weakest && rank != 1))) {
                weakest = rank;
                split = i;
                split_width = width;
                split_operator = op;
            }
            i += width - 1;
        }
        if (split != end) {
            auto result = make(begin, end, ExpressionKind::binary, split_operator);
            result.arguments.push_back(parse(begin, split));
            result.arguments.push_back(parse(split + split_width, end));
            return result;
        }
        if (value(begin) == "!" || value(begin) == "-" || value(begin) == "+" ||
            value(begin) == "await") {
            auto result = make(begin, end, ExpressionKind::unary, value(begin));
            result.arguments.push_back(parse(begin + 1, end));
            return result;
        }
        if (value(begin) == "(" && value(end - 1) == ")" &&
            matching_symbol(significant[begin], "(", ")") == significant[end - 1]) {
            auto result = make(begin, end, ExpressionKind::group);
            if (begin + 2 < end) result.arguments.push_back(parse(begin + 1, end - 1));
            return result;
        }
        if (value(begin) == "{" && value(end - 1) == "}" &&
            matching_symbol(significant[begin], "{", "}") == significant[end - 1]) {
            auto result = make(begin, end, ExpressionKind::object);
            auto start = begin + 1;
            int depth = 0;
            const auto append_entry = [&](std::size_t finish) {
                if (start >= finish) {
                    if (finish < end - 1)
                        error(finish, "Object entry is missing a key and value");
                    return;
                }
                for (auto colon = start + 1; colon + 1 < finish; ++colon) {
                    if (value(colon) != ":") continue;
                    auto entry = make(start, finish, ExpressionKind::entry);
                    entry.arguments.push_back(parse(start, colon));
                    entry.arguments.push_back(parse(colon + 1, finish));
                    result.arguments.push_back(std::move(entry));
                    return;
                }
                error(start, "Object entry requires a key, ':' and value");
                result.kind = ExpressionKind::raw;
                result.arguments.clear();
            };
            for (auto i = start; i < end - 1; ++i) {
                if (value(i) == "(" || value(i) == "[" || value(i) == "{") ++depth;
                else if (value(i) == ")" || value(i) == "]" || value(i) == "}") --depth;
                if (value(i) == "," && depth == 0) {
                    append_entry(i);
                    start = i + 1;
                }
            }
            append_entry(end - 1);
            return result;
        }
        if (value(begin) == "[" && value(end - 1) == "]" &&
            matching_symbol(significant[begin], "[", "]") == significant[end - 1]) {
            auto result = make(begin, end, ExpressionKind::list);
            auto start = begin + 1;
            int depth = 0;
            for (auto i = start; i < end - 1; ++i) {
                if (value(i) == "(" || value(i) == "[" || value(i) == "{") ++depth;
                else if (value(i) == ")" || value(i) == "]" || value(i) == "}") --depth;
                if (value(i) == "," && depth == 0) {
                    if (start < i) result.arguments.push_back(parse(start, i));
                    else error(i, "List element is missing");
                    start = i + 1;
                }
            }
            if (start < end - 1) result.arguments.push_back(parse(start, end - 1));
            return result;
        }
        // A postfix operation must close at the end of this expression.
        if (value(end - 1) == ")") {
            for (auto i = begin + 1; i + 1 < end; ++i) {
                if (value(i) != "(" ||
                    matching_symbol(significant[i], "(", ")") != significant[end - 1])
                    continue;
                auto result = make(begin, end, ExpressionKind::call);
                result.arguments.push_back(parse(begin, i));
                auto start = i + 1;
                int depth = 0;
                for (auto j = start; j < end - 1; ++j) {
                    if (value(j) == "(" || value(j) == "[" || value(j) == "{") ++depth;
                    else if (value(j) == ")" || value(j) == "]" || value(j) == "}") --depth;
                    if (value(j) == "," && depth == 0) {
                        if (start < j) result.arguments.push_back(parse(start, j));
                        else error(j, "Call argument is missing");
                        start = j + 1;
                    }
                }
                if (start < end - 1) result.arguments.push_back(parse(start, end - 1));
                return result;
            }
        }
        if (value(end - 1) == "]") {
            for (auto i = begin + 1; i + 1 < end; ++i) {
                if (value(i) != "[" ||
                    matching_symbol(significant[i], "[", "]") != significant[end - 1])
                    continue;
                auto result = make(begin, end, ExpressionKind::subscript);
                result.arguments.push_back(parse(begin, i));
                if (i + 1 < end - 1) result.arguments.push_back(parse(i + 1, end - 1));
                return result;
            }
        }
        if (end >= begin + 4 &&
            ((value(end - 3) == "-" && value(end - 2) == ">") ||
             (value(end - 3) == ":" && value(end - 2) == ":"))) {
            auto result = make(begin, end, ExpressionKind::member,
                               value(end - 3) + value(end - 2));
            result.arguments.push_back(parse(begin, end - 3));
            result.arguments.push_back(parse(end - 1, end));
            return result;
        }
        if (end >= begin + 3 && value(end - 2) == ".") {
            auto result = make(begin, end, ExpressionKind::member, value(end - 2));
            result.arguments.push_back(parse(begin, end - 2));
            result.arguments.push_back(parse(end - 1, end));
            return result;
        }
        return raw; // Native C++ interoperability remains source-preserving.
    };
    return parse(0, significant.size());
}

std::vector<MethodStatement> Parser::parse_method_body(
    std::size_t opening, std::size_t closing
) {
    std::vector<MethodStatement> statements;
    auto start = next_significant(opening);
    while (start && *start < closing) {
        auto cursor = *start;
        std::size_t braces = 0;
        std::size_t parentheses = 0;
        bool block = false;
        while (cursor < closing) {
            const auto& value = tokens_[cursor].lexeme;
            if (value == "(") ++parentheses;
            if (value == ")" && parentheses > 0) --parentheses;
            if (value == "{") ++braces;
            if (value == "}" && braces > 0) {
                --braces;
                if (braces == 0 && parentheses == 0) {
                    block = true;
                    break;
                }
            }
            if (value == ";" && braces == 0 && parentheses == 0) break;
            const auto next = next_significant(cursor);
            if (!next || *next >= closing) break;
            cursor = *next;
        }
        if (block) {
            auto brace = *start;
            while (brace < cursor && tokens_[brace].lexeme != "{") {
                const auto next = next_significant(brace);
                if (!next) break;
                brace = *next;
            }
            const auto control = tokens_[*start].lexeme;
            const bool conditional = control == "if";
            const bool loop = control == "while" || control == "for";
            MethodStatement statement{
                SourceSpan{tokens_[*start].offset,
                           tokens_[cursor].offset + tokens_[cursor].lexeme.size(),
                           tokens_[*start].line, tokens_[*start].column},
                conditional ? StatementKind::conditional :
                    loop ? StatementKind::loop_ : StatementKind::block,
                parse_expression(*start, cursor)
            };
            statement.name = control;
            if (brace < cursor) {
                statement.children = parse_method_body(brace, cursor);
                const auto open = next_significant(*start);
                if ((conditional || loop) && open && tokens_[*open].lexeme == "(") {
                    const auto close = matching_symbol(*open, "(", ")");
                    const auto first = next_significant(*open);
                    const auto last = close ? previous_significant(*close) : std::nullopt;
                    if (close && first && last && *first < *close && *last >= *first) {
                        if (control == "for") {
                            auto part_start = *first;
                            std::size_t depth = 0;
                            for (auto at = *first; at <= *last; ++at) {
                                if (tokens_[at].trivia()) continue;
                                const auto& symbol = tokens_[at].lexeme;
                                if (symbol == "(" || symbol == "[" || symbol == "{") ++depth;
                                else if ((symbol == ")" || symbol == "]" ||
                                          symbol == "}") && depth > 0) --depth;
                                if (symbol != ";" || depth != 0) continue;
                                const auto end = previous_significant(at);
                                statement.for_parts.push_back(
                                    end && *end >= part_start &&
                                        tokens_[part_start].lexeme != ";"
                                        ? parse_expression(part_start, *end) : Expression{});
                                if (statement.for_parts.size() == 1 && end &&
                                    tokens_[part_start].lexeme == "const") {
                                    const auto name = next_significant(part_start);
                                    const auto equals = name
                                        ? next_significant(*name) : std::nullopt;
                                    const auto initializer = equals
                                        ? next_significant(*equals) : std::nullopt;
                                    if (name && equals && initializer &&
                                        tokens_[*name].kind == TokenKind::identifier &&
                                        tokens_[*equals].lexeme == "=" &&
                                        *initializer <= *end) {
                                        statement.for_binding_name = tokens_[*name].lexeme;
                                        statement.for_binding_initializer =
                                            parse_expression(*initializer, *end);
                                    }
                                }
                                const auto next = next_significant(at);
                                part_start = next ? *next : *close;
                            }
                            statement.for_parts.push_back(
                                part_start < *close
                                    ? parse_expression(part_start, *last) : Expression{});
                            if (statement.for_parts.size() == 3)
                                statement.expression = statement.for_parts[1];
                        } else {
                            statement.expression = parse_expression(*first, *last);
                        }
                    }
                }
            }
            if (conditional) {
                const auto otherwise = next_significant(cursor);
                if (otherwise && *otherwise < closing &&
                    tokens_[*otherwise].lexeme == "else") {
                    const auto branch = next_significant(*otherwise);
                    if (branch && tokens_[*branch].lexeme == "{") {
                        const auto end = matching_symbol(*branch, "{", "}");
                        if (end && *end < closing) {
                            statement.alternative = parse_method_body(*branch, *end);
                            cursor = *end;
                            statement.span.end = tokens_[cursor].offset +
                                                 tokens_[cursor].lexeme.size();
                        }
                    } else if (branch && tokens_[*branch].lexeme == "if") {
                        // Parse an else-if as a nested conditional without copying tokens.
                        auto nested = *branch;
                        while (nested < closing && tokens_[nested].lexeme != "{") {
                            const auto next = next_significant(nested);
                            if (!next) break;
                            nested = *next;
                        }
                        auto end = nested < closing
                            ? matching_symbol(nested, "{", "}") : std::nullopt;
                        if (end && *end < closing) {
                            // Include the rest of the else-if chain in this branch.
                            while (true) {
                                const auto following = next_significant(*end);
                                if (!following || *following >= closing ||
                                    tokens_[*following].lexeme != "else") break;
                                const auto arm = next_significant(*following);
                                if (!arm || *arm >= closing) break;
                                auto arm_open = *arm;
                                if (tokens_[arm_open].lexeme == "if") {
                                    while (arm_open < closing &&
                                           tokens_[arm_open].lexeme != "{") {
                                        const auto next = next_significant(arm_open);
                                        if (!next) break;
                                        arm_open = *next;
                                    }
                                }
                                if (arm_open >= closing ||
                                    tokens_[arm_open].lexeme != "{") break;
                                const auto arm_end = matching_symbol(arm_open, "{", "}");
                                if (!arm_end || *arm_end >= closing) break;
                                end = arm_end;
                            }
                            statement.alternative = parse_method_body(*otherwise, *end + 1);
                            cursor = *end;
                            statement.span.end = tokens_[cursor].offset +
                                                 tokens_[cursor].lexeme.size();
                        }
                    }
                }
            }
            statements.push_back(std::move(statement));
        } else if (tokens_[cursor].lexeme == ";") {
            const auto expression_start = next_significant(*start);
            const auto expression_end = previous_significant(cursor);
            const auto& keyword = tokens_[*start].lexeme;
            const auto kind = keyword == "return"
                ? StatementKind::return_
                : keyword == "const" ? StatementKind::binding
                : keyword == "break" ? StatementKind::break_
                : keyword == "continue" ? StatementKind::continue_
                : StatementKind::expression;
            if ((kind == StatementKind::return_ || kind == StatementKind::break_ ||
                 kind == StatementKind::continue_) && expression_end &&
                *expression_end == *start) {
                statements.push_back(MethodStatement{
                    SourceSpan{tokens_[*start].offset, tokens_[cursor].offset + 1,
                               tokens_[*start].line, tokens_[*start].column},
                    kind, {}
                });
                start = next_significant(cursor);
                continue;
            }
            if (expression_start && expression_end &&
                *expression_start <= *expression_end) {
                auto value_start = kind == StatementKind::return_
                    ? *expression_start : *start;
                std::string binding_name;
                if (kind == StatementKind::binding) {
                    const auto equals = next_significant(*expression_start);
                    if (equals && tokens_[*equals].lexeme == "=") {
                        const auto initializer = next_significant(*equals);
                        if (initializer && *initializer < cursor) {
                            binding_name = tokens_[*expression_start].lexeme;
                            value_start = *initializer;
                        }
                    }
                }
                statements.push_back(MethodStatement{
                    SourceSpan{tokens_[*start].offset,
                               tokens_[cursor].offset + 1,
                               tokens_[*start].line, tokens_[*start].column},
                    kind,
                    parse_expression(value_start, *expression_end),
                    std::move(binding_name)
                });
            }
        }
        start = next_significant(cursor);
    }
    return statements;
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

void Parser::parse_model_members(
    const std::string& class_name,
    std::size_t body_open,
    std::size_t body_close
) {
    std::unordered_set<std::string> field_names;
    std::unordered_set<std::string> relationship_names;

    for (auto cursor = body_open + 1; cursor < body_close; ++cursor) {
        const auto& token = tokens_[cursor];

        if (token.trivia()) {
            continue;
        }

        const auto configuration =
            model_configuration_kind(token.lexeme);

        if (configuration) {
            const auto equals = next_significant(cursor);
            const auto value =
                equals ? next_significant(*equals) : std::nullopt;
            const auto semicolon =
                value ? next_significant(*value) : std::nullopt;

            if (
                !equals ||
                !value ||
                !semicolon ||
                *semicolon >= body_close ||
                tokens_[*equals].lexeme != "=" ||
                tokens_[*semicolon].lexeme != ";"
            ) {
                result_.diagnostics.push_back(Diagnostic{
                    DiagnosticLevel::error,
                    SourceLocation{source_name_, token.line, token.column},
                    "Invalid model configuration '" + token.lexeme + "'",
                    "GNR1101",
                    "Use '" + token.lexeme + " = value;'."
                });
                continue;
            }

            const bool string_configuration =
                *configuration == ModelConfigurationKind::table ||
                *configuration == ModelConfigurationKind::connection;

            if (
                string_configuration &&
                tokens_[*value].kind != TokenKind::string_literal
            ) {
                result_.diagnostics.push_back(Diagnostic{
                    DiagnosticLevel::error,
                    SourceLocation{
                        source_name_,
                        tokens_[*value].line,
                        tokens_[*value].column
                    },
                    token.lexeme + " must be a string",
                    "GNR1102",
                    "Use a quoted string value."
                });
                cursor = *semicolon;
                continue;
            }

            const bool enabled = tokens_[*value].lexeme == "true";
            const bool disabled = tokens_[*value].lexeme == "false";

            if (!string_configuration && !enabled && !disabled) {
                result_.diagnostics.push_back(Diagnostic{
                    DiagnosticLevel::error,
                    SourceLocation{
                        source_name_,
                        tokens_[*value].line,
                        tokens_[*value].column
                    },
                    token.lexeme + " must be true or false",
                    "GNR1103",
                    "Use a boolean literal."
                });
                cursor = *semicolon;
                continue;
            }

            result_.program.nodes.push_back(ModelConfiguration{
                SourceSpan{
                    token.offset,
                    tokens_[*semicolon].offset +
                        tokens_[*semicolon].lexeme.size(),
                    token.line,
                    token.column
                },
                class_name,
                *configuration,
                string_configuration
                    ? unquote(tokens_[*value].lexeme)
                    : tokens_[*value].lexeme,
                enabled
            });

            cursor = *semicolon;
            continue;
        }

        if (token.lexeme == "id") {
            const auto semicolon = next_significant(cursor);

            if (
                semicolon &&
                *semicolon < body_close &&
                tokens_[*semicolon].lexeme == ";"
            ) {
                if (!field_names.insert("id").second) {
                    result_.diagnostics.push_back(Diagnostic{
                        DiagnosticLevel::error,
                        SourceLocation{source_name_, token.line, token.column},
                        "Duplicate model field 'id'",
                        "GNR1110",
                        "Declare each model field only once."
                    });
                } else {
                    result_.program.nodes.push_back(ModelField{
                        SourceSpan{
                            token.offset,
                            tokens_[*semicolon].offset +
                                tokens_[*semicolon].lexeme.size(),
                            token.line,
                            token.column
                        },
                        token_span(token),
                        class_name,
                        "integer",
                        "id",
                        false,
                        true,
                        true
                    });
                }

                cursor = *semicolon;
                continue;
            }
        }

        if (token.kind == TokenKind::identifier) {
            const auto open_paren = next_significant(cursor);

            if (
                open_paren &&
                *open_paren < body_close &&
                tokens_[*open_paren].lexeme == "("
            ) {
                const auto close_paren =
                    matching_symbol(*open_paren, "(", ")");
                const auto first_parameter =
                    next_significant(*open_paren);

                if (
                    close_paren &&
                    first_parameter &&
                    *close_paren < body_close &&
                    *first_parameter == *close_paren
                ) {
                    const auto method_open =
                        next_significant(*close_paren);

                    if (
                        method_open &&
                        *method_open < body_close &&
                        tokens_[*method_open].lexeme == "{"
                    ) {
                        const auto method_close =
                            matching_symbol(*method_open, "{", "}");

                        if (
                            method_close &&
                            *method_close <= body_close
                        ) {
                            const auto returned =
                                next_significant(*method_open);
                            const auto factory =
                                returned &&
                                *returned < *method_close &&
                                tokens_[*returned].lexeme == "return"
                                    ? next_significant(*returned)
                                    : std::nullopt;
                            const auto relationship =
                                factory && *factory < *method_close
                                    ? model_relationship_kind(
                                        tokens_[*factory].lexeme
                                    )
                                    : std::nullopt;

                            if (relationship) {
                                const auto angle_open =
                                    next_significant(*factory);
                                const auto related =
                                    angle_open
                                        ? next_significant(*angle_open)
                                        : std::nullopt;

                                if (
                                    !angle_open ||
                                    !related ||
                                    *related >= *method_close ||
                                    tokens_[*angle_open].lexeme != "<" ||
                                    tokens_[*related].kind !=
                                        TokenKind::identifier
                                ) {
                                    result_.diagnostics.push_back(Diagnostic{
                                        DiagnosticLevel::error,
                                        SourceLocation{
                                            source_name_,
                                            tokens_[*factory].line,
                                            tokens_[*factory].column
                                        },
                                        "Invalid model relationship declaration",
                                        "GNR1120",
                                        "Use syntax like 'posts() { return hasMany<Post>(); }'."
                                    });
                                    cursor = *method_close;
                                    continue;
                                }

                                std::string through_type;
                                auto angle_close =
                                    next_significant(*related);

                                if (
                                    angle_close &&
                                    *angle_close < *method_close &&
                                    tokens_[*angle_close].lexeme == ","
                                ) {
                                    const auto through =
                                        next_significant(*angle_close);

                                    if (
                                        through &&
                                        *through < *method_close &&
                                        tokens_[*through].kind ==
                                            TokenKind::identifier
                                    ) {
                                        through_type =
                                            tokens_[*through].lexeme;
                                        angle_close =
                                            next_significant(*through);
                                    }
                                }

                                if (
                                    !angle_close ||
                                    *angle_close >= *method_close ||
                                    tokens_[*angle_close].lexeme != ">" ||
                                    (
                                        relationship_requires_through(
                                            *relationship
                                        ) &&
                                        through_type.empty()
                                    )
                                ) {
                                    result_.diagnostics.push_back(Diagnostic{
                                        DiagnosticLevel::error,
                                        SourceLocation{
                                            source_name_,
                                            tokens_[*factory].line,
                                            tokens_[*factory].column
                                        },
                                        "Invalid relationship type arguments",
                                        "GNR1121",
                                        "Through relationships require both related and through model types."
                                    });
                                    cursor = *method_close;
                                    continue;
                                }

                                const auto args_open =
                                    next_significant(*angle_close);
                                const auto args_close =
                                    args_open &&
                                    *args_open < *method_close &&
                                    tokens_[*args_open].lexeme == "("
                                        ? matching_symbol(
                                            *args_open,
                                            "(",
                                            ")"
                                        )
                                        : std::nullopt;
                                const auto semicolon =
                                    args_close
                                        ? next_significant(*args_close)
                                        : std::nullopt;
                                const auto after_return =
                                    semicolon
                                        ? next_significant(*semicolon)
                                        : std::nullopt;

                                if (
                                    !args_open ||
                                    !args_close ||
                                    !semicolon ||
                                    !after_return ||
                                    *args_close >= *method_close ||
                                    *semicolon >= *method_close ||
                                    tokens_[*semicolon].lexeme != ";" ||
                                    *after_return != *method_close
                                ) {
                                    result_.diagnostics.push_back(Diagnostic{
                                        DiagnosticLevel::error,
                                        SourceLocation{
                                            source_name_,
                                            tokens_[*factory].line,
                                            tokens_[*factory].column
                                        },
                                        "Relationship method must return one relationship expression",
                                        "GNR1122",
                                        "Keep the relationship body to a single return statement."
                                    });
                                    cursor = *method_close;
                                    continue;
                                }

                                std::vector<std::string> arguments;
                                bool valid_arguments = true;

                                for (
                                    auto argument = *args_open + 1;
                                    argument < *args_close;
                                    ++argument
                                ) {
                                    if (tokens_[argument].trivia()) {
                                        continue;
                                    }

                                    if (tokens_[argument].lexeme == ",") {
                                        continue;
                                    }

                                    if (
                                        tokens_[argument].kind !=
                                        TokenKind::string_literal
                                    ) {
                                        valid_arguments = false;
                                        break;
                                    }

                                    arguments.push_back(
                                        unquote(tokens_[argument].lexeme)
                                    );
                                }

                                if (!valid_arguments) {
                                    result_.diagnostics.push_back(Diagnostic{
                                        DiagnosticLevel::error,
                                        SourceLocation{
                                            source_name_,
                                            tokens_[*args_open].line,
                                            tokens_[*args_open].column
                                        },
                                        "Relationship key overrides must be string literals",
                                        "GNR1123",
                                        "Use quoted column or pivot names in relationship arguments."
                                    });
                                    cursor = *method_close;
                                    continue;
                                }

                                if (
                                    !relationship_names.insert(
                                        token.lexeme
                                    ).second
                                ) {
                                    result_.diagnostics.push_back(Diagnostic{
                                        DiagnosticLevel::error,
                                        SourceLocation{
                                            source_name_,
                                            token.line,
                                            token.column
                                        },
                                        "Duplicate model relationship '" +
                                            token.lexeme + "'",
                                        "GNR1124",
                                        "Declare each model relationship only once."
                                    });
                                    cursor = *method_close;
                                    continue;
                                }

                                result_.program.nodes.push_back(
                                    ModelRelationship{
                                        SourceSpan{
                                            token.offset,
                                            tokens_[*method_close].offset +
                                                tokens_[*method_close].lexeme.size(),
                                            token.line,
                                            token.column
                                        },
                                        class_name,
                                        token.lexeme,
                                        *relationship,
                                        tokens_[*related].lexeme,
                                        through_type,
                                        std::move(arguments)
                                    }
                                );

                                cursor = *method_close;
                                continue;
                            }
                        }
                    }
                }
            }
        }

        if (!scalar_type(token.lexeme)) {
            continue;
        }

        auto name = next_significant(cursor);
        bool nullable = false;

        if (name && tokens_[*name].lexeme == "?") {
            nullable = true;
            name = next_significant(*name);
        }

        if (
            !name ||
            *name >= body_close ||
            tokens_[*name].kind != TokenKind::identifier
        ) {
            continue;
        }

        const auto marker = next_significant(*name);
        if (
            !marker ||
            *marker >= body_close ||
            (
                tokens_[*marker].lexeme != ";" &&
                tokens_[*marker].lexeme != "="
            )
        ) {
            continue;
        }

        const auto field_name = tokens_[*name].lexeme;

        if (!field_names.insert(field_name).second) {
            result_.diagnostics.push_back(Diagnostic{
                DiagnosticLevel::error,
                SourceLocation{
                    source_name_,
                    tokens_[*name].line,
                    tokens_[*name].column
                },
                "Duplicate model field '" + field_name + "'",
                "GNR1110",
                "Declare each model field only once."
            });
            continue;
        }

        result_.program.nodes.push_back(ModelField{
            SourceSpan{
                token.offset,
                tokens_[*name].offset + tokens_[*name].lexeme.size(),
                token.line,
                token.column
            },
            SourceSpan{
                token.offset,
                tokens_[*name].offset,
                token.line,
                token.column
            },
            class_name,
            token.lexeme,
            field_name,
            nullable,
            field_name == "id",
            false
        });

        cursor = *name;
    }
}

void Parser::parse_controller_members(
    const std::string& class_name,
    std::size_t body_open,
    std::size_t body_close
) {
    std::unordered_set<std::string> injected_names;

    for (auto cursor = body_open + 1; cursor < body_close; ++cursor) {
        const auto& token = tokens_[cursor];

        if (token.trivia()) {
            continue;
        }

        if (token.lexeme == "inject") {
            const auto type = next_significant(cursor);
            const auto name =
                type ? next_significant(*type) : std::nullopt;
            const auto semicolon =
                name ? next_significant(*name) : std::nullopt;

            if (
                !type ||
                !name ||
                !semicolon ||
                *semicolon >= body_close ||
                !tokens_[*type].word() ||
                tokens_[*name].kind != TokenKind::identifier ||
                tokens_[*semicolon].lexeme != ";"
            ) {
                result_.diagnostics.push_back(Diagnostic{
                    DiagnosticLevel::error,
                    SourceLocation{source_name_, token.line, token.column},
                    "inject requires: inject Type name;",
                    "GNR1201",
                    "Declare an injected dependency with a type and field name."
                });
                continue;
            }

            if (!injected_names.insert(tokens_[*name].lexeme).second) {
                result_.diagnostics.push_back(Diagnostic{
                    DiagnosticLevel::error,
                    SourceLocation{
                        source_name_,
                        tokens_[*name].line,
                        tokens_[*name].column
                    },
                    "Duplicate injected dependency '" +
                        tokens_[*name].lexeme + "'",
                    "GNR1202",
                    "Use a unique name for each injected dependency."
                });
                cursor = *semicolon;
                continue;
            }

            result_.program.nodes.push_back(InjectDeclaration{
                SourceSpan{
                    token.offset,
                    tokens_[*name].offset + tokens_[*name].lexeme.size(),
                    token.line,
                    token.column
                },
                token_span(tokens_[*type]),
                token_span(tokens_[*name]),
                class_name,
                tokens_[*type].lexeme,
                tokens_[*name].lexeme
            });

            cursor = *semicolon;
            continue;
        }

        std::size_t return_type = cursor;
        bool asynchronous = false;

        if (token.lexeme == "async") {
            asynchronous = true;
            const auto next = next_significant(cursor);

            if (!next || *next >= body_close) {
                continue;
            }

            return_type = *next;
        }

        if (!tokens_[return_type].word()) {
            continue;
        }

        const auto name = next_significant(return_type);
        const auto open =
            name ? next_significant(*name) : std::nullopt;

        if (
            !name ||
            !open ||
            *open >= body_close ||
            tokens_[*name].kind != TokenKind::identifier ||
            tokens_[*open].lexeme != "("
        ) {
            continue;
        }

        const auto close = matching_symbol(*open, "(", ")");
        if (!close || *close >= body_close) {
            continue;
        }

        const auto method_open = next_significant(*close);
        if (
            !method_open ||
            *method_open >= body_close ||
            tokens_[*method_open].lexeme != "{"
        ) {
            continue;
        }

        const auto method_close =
            matching_symbol(*method_open, "{", "}");

        if (!method_close || *method_close > body_close) {
            continue;
        }

        result_.program.nodes.push_back(ControllerMethod{
            SourceSpan{
                token.offset,
                tokens_[*method_close].offset +
                    tokens_[*method_close].lexeme.size(),
                token.line,
                token.column
            },
            token_span(tokens_[return_type]),
            token_span(tokens_[*name]),
            class_name,
            tokens_[return_type].lexeme,
            tokens_[*name].lexeme,
            asynchronous,
            parse_parameters(*open, *close),
            parse_method_body(*method_open, *method_close)
        });

        cursor = *method_close;
    }
}

void Parser::parse_route_declaration(
    std::size_t index
) {
    const auto& route = tokens_[index];
    if (route.lexeme != "Route") {
        return;
    }

    const auto colon_one = next_significant(index);
    const auto colon_two =
        colon_one ? next_significant(*colon_one) : std::nullopt;
    const auto method =
        colon_two ? next_significant(*colon_two) : std::nullopt;
    const auto open =
        method ? next_significant(*method) : std::nullopt;

    if (
        !colon_one ||
        !colon_two ||
        !method ||
        !open ||
        tokens_[*colon_one].lexeme != ":" ||
        tokens_[*colon_two].lexeme != ":" ||
        tokens_[*open].lexeme != "("
    ) {
        return;
    }

    const auto method_kind =
        route_method_kind(tokens_[*method].lexeme);
    if (!method_kind) {
        return;
    }

    const auto close = matching_symbol(*open, "(", ")");
    if (!close) {
        result_.diagnostics.push_back(Diagnostic{
            DiagnosticLevel::error,
            SourceLocation{
                source_name_,
                tokens_[*open].line,
                tokens_[*open].column
            },
            "Route call is missing a closing parenthesis",
            "GNR1301",
            "Close the Route call with ')'."
        });
        return;
    }

    std::optional<std::size_t> comma;
    std::size_t nested_parentheses = 0;
    std::size_t nested_braces = 0;
    std::size_t nested_brackets = 0;

    for (auto cursor = *open + 1; cursor < *close; ++cursor) {
        if (tokens_[cursor].trivia()) {
            continue;
        }

        const auto& lexeme = tokens_[cursor].lexeme;

        if (lexeme == "(") {
            ++nested_parentheses;
        } else if (lexeme == ")") {
            if (nested_parentheses > 0) {
                --nested_parentheses;
            }
        } else if (lexeme == "{") {
            ++nested_braces;
        } else if (lexeme == "}") {
            if (nested_braces > 0) {
                --nested_braces;
            }
        } else if (lexeme == "[") {
            ++nested_brackets;
        } else if (lexeme == "]") {
            if (nested_brackets > 0) {
                --nested_brackets;
            }
        } else if (
            lexeme == "," &&
            nested_parentheses == 0 &&
            nested_braces == 0 &&
            nested_brackets == 0
        ) {
            comma = cursor;
            break;
        }
    }

    if (!comma) {
        result_.diagnostics.push_back(Diagnostic{
            DiagnosticLevel::error,
            SourceLocation{
                source_name_,
                route.line,
                route.column
            },
            "Route requires a URI and controller action",
            "GNR1302",
            "Use Route::get(\"/path\", Controller::action)."
        });
        return;
    }

    const auto controller = next_significant(*comma);
    const auto handler_colon_one =
        controller ? next_significant(*controller) : std::nullopt;
    const auto handler_colon_two =
        handler_colon_one
            ? next_significant(*handler_colon_one)
            : std::nullopt;
    const auto action =
        handler_colon_two
            ? next_significant(*handler_colon_two)
            : std::nullopt;

    if (
        !controller ||
        !handler_colon_one ||
        !handler_colon_two ||
        !action ||
        *action >= *close ||
        tokens_[*controller].kind != TokenKind::identifier ||
        tokens_[*handler_colon_one].lexeme != ":" ||
        tokens_[*handler_colon_two].lexeme != ":" ||
        tokens_[*action].kind != TokenKind::identifier
    ) {
        result_.diagnostics.push_back(Diagnostic{
            DiagnosticLevel::error,
            SourceLocation{
                source_name_,
                tokens_[*comma].line,
                tokens_[*comma].column
            },
            "Route handler must be a controller action",
            "GNR1303",
            "Use a handler like UserController::index."
        });
        return;
    }

    SourceSpan middleware_span{};
    std::string middleware_type;
    bool has_middleware = false;

    const auto route_dot = next_significant(*close);
    const auto middleware_name =
        route_dot ? next_significant(*route_dot) : std::nullopt;
    const auto middleware_open =
        middleware_name
            ? next_significant(*middleware_name)
            : std::nullopt;

    if (
        route_dot &&
        middleware_name &&
        middleware_open &&
        tokens_[*route_dot].lexeme == "." &&
        tokens_[*middleware_name].lexeme == "middleware" &&
        tokens_[*middleware_open].lexeme == "("
    ) {
        const auto middleware_close =
            matching_symbol(*middleware_open, "(", ")");

        if (!middleware_close) {
            result_.diagnostics.push_back(Diagnostic{
                DiagnosticLevel::error,
                SourceLocation{
                    source_name_,
                    tokens_[*middleware_open].line,
                    tokens_[*middleware_open].column
                },
                "Route middleware call is missing a closing parenthesis",
                "GNR1304",
                "Close the middleware call with ')'."
            });
            return;
        }

        const auto middleware_type_index =
            next_significant(*middleware_open);
        const auto after_type =
            middleware_type_index
                ? next_significant(*middleware_type_index)
                : std::nullopt;

        if (
            !middleware_type_index ||
            *middleware_type_index >= *middleware_close ||
            tokens_[*middleware_type_index].kind !=
                TokenKind::identifier ||
            !after_type ||
            *after_type != *middleware_close
        ) {
            result_.diagnostics.push_back(Diagnostic{
                DiagnosticLevel::error,
                SourceLocation{
                    source_name_,
                    tokens_[*middleware_open].line,
                    tokens_[*middleware_open].column
                },
                "Route middleware requires one middleware type",
                "GNR1305",
                "Use .middleware(AuthMiddleware)."
            });
            return;
        }

        middleware_span = SourceSpan{
            tokens_[*middleware_name].offset,
            tokens_[*middleware_close].offset +
                tokens_[*middleware_close].lexeme.size(),
            tokens_[*middleware_name].line,
            tokens_[*middleware_name].column
        };
        middleware_type =
            tokens_[*middleware_type_index].lexeme;
        has_middleware = true;
    }

    result_.program.nodes.push_back(RouteDeclaration{
        token_span(route),
        token_span(tokens_[*method]),
        SourceSpan{
            tokens_[*controller].offset,
            tokens_[*controller].offset,
            tokens_[*controller].line,
            tokens_[*controller].column
        },
        middleware_span,
        *method_kind,
        tokens_[*controller].lexeme,
        tokens_[*action].lexeme,
        middleware_type,
        has_middleware
    });
}

void Parser::parse_framework_methods(
    const std::string& class_name,
    FrameworkBaseKind kind,
    std::size_t body_open,
    std::size_t body_close
) {
    for (auto cursor = body_open + 1; cursor < body_close; ++cursor) {
        const auto& token = tokens_[cursor];
        if (token.trivia()) {
            continue;
        }

        std::size_t return_type = cursor;
        bool asynchronous = false;

        if (token.lexeme == "async") {
            asynchronous = true;
            const auto next = next_significant(cursor);
            if (!next || *next >= body_close) {
                continue;
            }
            return_type = *next;
        }

        if (!tokens_[return_type].word()) {
            continue;
        }

        const auto name = next_significant(return_type);
        const auto open = name ? next_significant(*name) : std::nullopt;

        if (
            !name || !open || *open >= body_close ||
            tokens_[*name].kind != TokenKind::identifier ||
            tokens_[*open].lexeme != "("
        ) {
            continue;
        }

        const auto close = matching_symbol(*open, "(", ")");
        if (!close || *close >= body_close) {
            continue;
        }

        const auto method_open = next_significant(*close);
        if (
            !method_open || *method_open >= body_close ||
            tokens_[*method_open].lexeme != "{"
        ) {
            continue;
        }

        const auto method_close = matching_symbol(*method_open, "{", "}");
        if (!method_close || *method_close > body_close) {
            continue;
        }

        result_.program.nodes.push_back(FrameworkMethod{
            SourceSpan{
                token.offset,
                tokens_[*method_close].offset +
                    tokens_[*method_close].lexeme.size(),
                token.line,
                token.column
            },
            token_span(tokens_[return_type]),
            token_span(tokens_[*name]),
            class_name,
            kind,
            tokens_[return_type].lexeme,
            tokens_[*name].lexeme,
            asynchronous,
            parse_parameters(*open, *close),
            parse_method_body(*method_open, *method_close)
        });

        cursor = *method_close;
    }
}

void Parser::parse_framework_members(
    const std::string& class_name,
    FrameworkBaseKind kind,
    std::size_t body_open,
    std::size_t body_close
) {
    if (kind == FrameworkBaseKind::model) {
        parse_model_members(class_name, body_open, body_close);
        return;
    }

    if (kind == FrameworkBaseKind::controller) {
        parse_controller_members(class_name, body_open, body_close);
        return;
    }

    parse_framework_methods(
        class_name,
        kind,
        body_open,
        body_close
    );
}

void Parser::parse_framework_declaration(std::size_t index) {
    const auto kind = framework_kind(tokens_[index].lexeme);
    if (!kind || !statement_start(index) || scopes_.size() != 1) {
        return;
    }

    const auto name = next_significant(index);
    if (!name || tokens_[*name].kind != TokenKind::identifier) {
        result_.diagnostics.push_back(Diagnostic{
            DiagnosticLevel::error,
            SourceLocation{
                source_name_,
                tokens_[index].line,
                tokens_[index].column
            },
            declaration_name(*kind) + " declaration requires a name",
            "GNR1001",
            "Use syntax like '" + tokens_[index].lexeme + " User { }'."
        });
        return;
    }

    const auto body = next_significant(*name);
    if (!body || tokens_[*body].lexeme != "{") {
        result_.diagnostics.push_back(Diagnostic{
            DiagnosticLevel::error,
            SourceLocation{
                source_name_,
                tokens_[*name].line,
                tokens_[*name].column
            },
            declaration_name(*kind) + " declaration requires a body",
            "GNR1002",
            "Add a '{ ... }' body after the declaration name."
        });
        return;
    }

    const auto body_close = matching_symbol(*body, "{", "}");
    if (!body_close) {
        result_.diagnostics.push_back(Diagnostic{
            DiagnosticLevel::error,
            SourceLocation{
                source_name_,
                tokens_[*body].line,
                tokens_[*body].column
            },
            declaration_name(*kind) + " body is missing a closing brace",
            "GNR1003",
            "Close the declaration body with '}'."
        });
        return;
    }

    if (!framework_names_.insert(tokens_[*name].lexeme).second) {
        result_.diagnostics.push_back(Diagnostic{
            DiagnosticLevel::error,
            SourceLocation{source_name_, tokens_[*name].line,
                           tokens_[*name].column},
            "Duplicate framework declaration '" + tokens_[*name].lexeme + "'",
            "GNR1004",
            "Give each framework declaration a unique name."
        });
    }

    const auto after_body = next_significant(*body_close);
    const bool has_semicolon =
        after_body && tokens_[*after_body].lexeme == ";";

    const auto declaration_index = result_.program.nodes.size();
    result_.program.nodes.push_back(FrameworkDeclaration{
        token_span(tokens_[index]),
        SourceSpan{
            tokens_[*name].offset + tokens_[*name].lexeme.size(),
            tokens_[*name].offset + tokens_[*name].lexeme.size(),
            tokens_[*name].line,
            tokens_[*name].column + tokens_[*name].lexeme.size()
        },
        SourceSpan{
            tokens_[*body_close].offset +
                tokens_[*body_close].lexeme.size(),
            tokens_[*body_close].offset +
                tokens_[*body_close].lexeme.size(),
            tokens_[*body_close].line,
            tokens_[*body_close].column +
                tokens_[*body_close].lexeme.size()
        },
        tokens_[*name].lexeme,
        *kind,
        !has_semicolon,
        SourceSpan{tokens_[index].offset,
                   tokens_[*body_close].offset + tokens_[*body_close].lexeme.size(),
                   tokens_[index].line, tokens_[index].column},
        {},
        token_span(tokens_[*body])
    });

    const auto members_begin = result_.program.nodes.size();
    parse_framework_members(
        tokens_[*name].lexeme,
        *kind,
        *body,
        *body_close
    );
    auto& declaration = std::get<FrameworkDeclaration>(
        result_.program.nodes[declaration_index]
    );
    for (auto member = members_begin; member < result_.program.nodes.size(); ++member) {
        declaration.members.push_back(member);
    }
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
    framework_names_.clear();
    result_ = {};

    for (std::size_t index = 0; index < tokens_.size(); ++index) {
        const auto& token = tokens_[index];

        if (token.trivia() || token.kind == TokenKind::end) {
            continue;
        }

        if (token.kind == TokenKind::identifier && token.lexeme == "Application") {
            const auto previous = previous_significant(index);
            if (!previous || tokens_[*previous].lexeme != ":") {
                result_.program.nodes.push_back(ApplicationReference{
                    SourceSpan{token.offset, token.offset + token.lexeme.size(),
                               token.line, token.column}
                });
            }
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

        parse_framework_declaration(index);
        parse_route_declaration(index);

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
            if (!base || !tokens_[*base].word()) {
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
            } else if (tokens_[*base].lexeme == "Middleware") {
                kind = FrameworkBaseKind::middleware;
            } else if (tokens_[*base].lexeme == "Policy") {
                kind = FrameworkBaseKind::policy;
            } else if (tokens_[*base].lexeme == "Event") {
                kind = FrameworkBaseKind::event;
            } else if (tokens_[*base].lexeme == "Listener") {
                kind = FrameworkBaseKind::listener;
            } else if (tokens_[*base].lexeme == "Notification") {
                kind = FrameworkBaseKind::notification;
            } else if (tokens_[*base].lexeme == "Mail") {
                kind = FrameworkBaseKind::mail;
            } else {
                framework_base = false;
            }

            if (framework_base) {
                const auto base_index = result_.program.nodes.size();
                result_.program.nodes.push_back(FrameworkBase{
                    token_span(tokens_[*base]),
                    tokens_[*class_name].lexeme,
                    kind
                });

                const auto body = next_significant(*base);
                if (body && tokens_[*body].lexeme == "{") {
                    const auto body_close =
                        matching_symbol(*body, "{", "}");

                    if (body_close) {
                        const auto members_begin = result_.program.nodes.size();
                        parse_framework_members(
                            tokens_[*class_name].lexeme,
                            kind,
                            *body,
                            *body_close
                        );
                        auto& declaration = std::get<FrameworkBase>(
                            result_.program.nodes[base_index]
                        );
                        declaration.declaration_span = SourceSpan{
                            token.offset,
                            tokens_[*body_close].offset +
                                tokens_[*body_close].lexeme.size(),
                            token.line, token.column
                        };
                        declaration.body_open_span = token_span(tokens_[*body]);
                        for (auto member = members_begin;
                             member < result_.program.nodes.size(); ++member) {
                            declaration.members.push_back(member);
                        }
                    }
                }
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
