#include <gungnir/language/controller_lowering.hpp>

#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gungnir/language/lexer.hpp>

namespace gungnir::language {

namespace {

struct Injection {
    std::string type;
    std::string name;
    std::size_t begin{0};
    std::size_t end{0};
};

struct ControllerInfo {
    std::string name;
    std::size_t body_open_end{0};
    std::size_t body_close_offset{0};
    std::size_t metadata_offset{0};
    bool needs_semicolon{false};
    std::vector<Injection> injections;
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
    ControllerLoweringResult& result,
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

std::string controller_prelude(const ControllerInfo& controller) {
    std::string result{"\npublic:\n"};

    if (controller.injections.empty()) {
        return result;
    }

    result += "    explicit " + controller.name +
              "(gungnir::Container& __gungnir_container)\n";
    result += "        : ";

    for (std::size_t index = 0; index < controller.injections.size(); ++index) {
        const auto& injection = controller.injections[index];

        if (index != 0) {
            result += ",\n          ";
        }

        result += injection.name +
                  "(__gungnir_container.resolve<" +
                  injection.type + ">())";
    }

    result += " {}\n";
    return result;
}

bool route_method(std::string_view name) {
    return
        name == "get" ||
        name == "post" ||
        name == "put" ||
        name == "patch" ||
        name == "delete" ||
        name == "remove" ||
        name == "options" ||
        name == "head";
}

} // namespace

ControllerLoweringResult ControllerLowerer::lower(
    std::string_view source,
    std::string source_name
) const {
    ControllerLoweringResult result;

    Lexer lexer{source};
    const auto tokens = lexer.tokenize();

    std::vector<ControllerInfo> controllers;

    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (tokens[index].trivia() || tokens[index].lexeme != "class") {
            continue;
        }

        const auto name = next_significant(tokens, index);
        const auto colon = name ? next_significant(tokens, *name) : std::nullopt;
        const auto base = colon ? next_significant(tokens, *colon) : std::nullopt;

        if (
            !name ||
            !colon ||
            !base ||
            tokens[*name].kind != TokenKind::identifier ||
            tokens[*colon].lexeme != ":" ||
            tokens[*base].lexeme != "Controller"
        ) {
            continue;
        }

        const auto body_open = next_significant(tokens, *base);
        if (!body_open || tokens[*body_open].lexeme != "{") {
            add_error(
                result,
                source_name,
                tokens[*base],
                "Controller declaration requires a body"
            );
            continue;
        }

        const auto body_close = matching_symbol(tokens, *body_open, "{", "}");
        if (!body_close) {
            add_error(
                result,
                source_name,
                tokens[*body_open],
                "Controller body is missing a closing brace"
            );
            continue;
        }

        ControllerInfo controller;
        controller.name = tokens[*name].lexeme;
        controller.body_open_end =
            tokens[*body_open].offset + tokens[*body_open].lexeme.size();
        controller.body_close_offset = tokens[*body_close].offset;

        const auto after_close = next_significant(tokens, *body_close);
        if (after_close && tokens[*after_close].lexeme == ";") {
            controller.metadata_offset =
                tokens[*after_close].offset + tokens[*after_close].lexeme.size();
        } else {
            controller.metadata_offset =
                tokens[*body_close].offset + tokens[*body_close].lexeme.size();
            controller.needs_semicolon = true;
        }

        std::size_t depth = 0;
        std::unordered_set<std::string> injected_names;

        for (
            std::size_t cursor = *body_open + 1;
            cursor < *body_close;
            ++cursor
        ) {
            const auto& token = tokens[cursor];

            if (token.trivia()) {
                continue;
            }

            if (token.lexeme == "{") {
                ++depth;
                continue;
            }

            if (token.lexeme == "}") {
                if (depth > 0) {
                    --depth;
                }
                continue;
            }

            if (depth != 0 || token.lexeme != "inject") {
                continue;
            }

            const auto type = next_significant(tokens, cursor);
            const auto field = type
                ? next_significant(tokens, *type)
                : std::nullopt;
            const auto semicolon = field
                ? next_significant(tokens, *field)
                : std::nullopt;

            if (
                !type ||
                !field ||
                !semicolon ||
                tokens[*type].kind != TokenKind::identifier ||
                tokens[*field].kind != TokenKind::identifier ||
                tokens[*semicolon].lexeme != ";"
            ) {
                add_error(
                    result,
                    source_name,
                    token,
                    "inject requires: inject Type name;"
                );
                continue;
            }

            if (!injected_names.insert(tokens[*field].lexeme).second) {
                add_error(
                    result,
                    source_name,
                    tokens[*field],
                    "Duplicate injected dependency '" +
                        tokens[*field].lexeme + "'"
                );
                continue;
            }

            controller.injections.push_back(Injection{
                tokens[*type].lexeme,
                tokens[*field].lexeme,
                token.offset,
                tokens[*field].offset + tokens[*field].lexeme.size()
            });

            result.edits.push_back(SourceEdit{
                token.offset,
                tokens[*field].offset + tokens[*field].lexeme.size(),
                "std::shared_ptr<" + tokens[*type].lexeme + "> " +
                    tokens[*field].lexeme
            });
        }

        for (const auto& injection : controller.injections) {
            for (
                std::size_t cursor = *body_open + 1;
                cursor < *body_close;
                ++cursor
            ) {
                if (
                    tokens[cursor].kind != TokenKind::identifier ||
                    tokens[cursor].lexeme != injection.name
                ) {
                    continue;
                }

                if (
                    tokens[cursor].offset >= injection.begin &&
                    tokens[cursor].offset < injection.end
                ) {
                    continue;
                }

                const auto dot = next_significant(tokens, cursor);
                if (dot && tokens[*dot].lexeme == ".") {
                    result.edits.push_back(SourceEdit{
                        tokens[*dot].offset,
                        tokens[*dot].offset + 1,
                        "->"
                    });
                }
            }
        }

        result.edits.push_back(SourceEdit{
            controller.body_open_end,
            controller.body_open_end,
            controller_prelude(controller)
        });

        if (controller.needs_semicolon) {
            result.edits.push_back(SourceEdit{
                controller.metadata_offset,
                controller.metadata_offset,
                ";"
            });
        }

        controllers.push_back(std::move(controller));
        index = *body_close;
    }

    // Route::get("/users", UserController::index)
    // -> gungnir::Route::get<UserController>(
    //        "/users", &UserController::index
    //    )
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (
            tokens[index].kind != TokenKind::identifier ||
            tokens[index].lexeme != "Route"
        ) {
            continue;
        }

        const auto colon_one = next_significant(tokens, index);
        const auto colon_two = colon_one
            ? next_significant(tokens, *colon_one)
            : std::nullopt;
        const auto method = colon_two
            ? next_significant(tokens, *colon_two)
            : std::nullopt;
        const auto open = method
            ? next_significant(tokens, *method)
            : std::nullopt;

        if (
            !colon_one ||
            !colon_two ||
            !method ||
            !open ||
            tokens[*colon_one].lexeme != ":" ||
            tokens[*colon_two].lexeme != ":" ||
            !route_method(tokens[*method].lexeme) ||
            tokens[*open].lexeme != "("
        ) {
            continue;
        }

        const auto close = matching_symbol(tokens, *open, "(", ")");
        if (!close) {
            add_error(
                result,
                source_name,
                tokens[*open],
                "Route call is missing a closing parenthesis"
            );
            continue;
        }

        std::optional<std::size_t> comma;
        std::size_t nested = 0;

        for (auto cursor = *open + 1; cursor < *close; ++cursor) {
            if (tokens[cursor].trivia()) {
                continue;
            }

            if (tokens[cursor].lexeme == "(") {
                ++nested;
            } else if (tokens[cursor].lexeme == ")") {
                if (nested > 0) {
                    --nested;
                }
            } else if (tokens[cursor].lexeme == "," && nested == 0) {
                comma = cursor;
                break;
            }
        }

        if (!comma) {
            continue;
        }

        const auto controller = next_significant(tokens, *comma);
        const auto handler_colon_one = controller
            ? next_significant(tokens, *controller)
            : std::nullopt;
        const auto handler_colon_two = handler_colon_one
            ? next_significant(tokens, *handler_colon_one)
            : std::nullopt;
        const auto action = handler_colon_two
            ? next_significant(tokens, *handler_colon_two)
            : std::nullopt;

        if (
            !controller ||
            !handler_colon_one ||
            !handler_colon_two ||
            !action ||
            tokens[*controller].kind != TokenKind::identifier ||
            tokens[*handler_colon_one].lexeme != ":" ||
            tokens[*handler_colon_two].lexeme != ":" ||
            tokens[*action].kind != TokenKind::identifier
        ) {
            continue;
        }

        result.edits.push_back(SourceEdit{
            tokens[index].offset,
            tokens[index].offset + tokens[index].lexeme.size(),
            "gungnir::Route"
        });

        result.edits.push_back(SourceEdit{
            tokens[*method].offset + tokens[*method].lexeme.size(),
            tokens[*method].offset + tokens[*method].lexeme.size(),
            "<" + tokens[*controller].lexeme + ">"
        });

        result.edits.push_back(SourceEdit{
            tokens[*controller].offset,
            tokens[*controller].offset,
            "&"
        });

        const auto route_dot =
            next_significant(tokens, *close);
        const auto middleware_name = route_dot
            ? next_significant(tokens, *route_dot)
            : std::nullopt;
        const auto middleware_open = middleware_name
            ? next_significant(tokens, *middleware_name)
            : std::nullopt;

        if (
            route_dot &&
            middleware_name &&
            middleware_open &&
            tokens[*route_dot].lexeme == "." &&
            tokens[*middleware_name].lexeme == "middleware" &&
            tokens[*middleware_open].lexeme == "("
        ) {
            const auto middleware_close =
                matching_symbol(
                    tokens,
                    *middleware_open,
                    "(",
                    ")"
                );

            if (middleware_close) {
                const auto middleware_type =
                    next_significant(
                        tokens,
                        *middleware_open
                    );

                if (
                    middleware_type &&
                    *middleware_type < *middleware_close &&
                    tokens[*middleware_type].kind ==
                        TokenKind::identifier
                ) {
                    const auto after_type =
                        next_significant(
                            tokens,
                            *middleware_type
                        );

                    if (
                        !after_type ||
                        *after_type == *middleware_close
                    ) {
                        result.edits.push_back(SourceEdit{
                            tokens[*middleware_name].offset,
                            tokens[*middleware_close].offset +
                                tokens[*middleware_close].lexeme.size(),
                            "middleware<" +
                                tokens[*middleware_type].lexeme +
                                ">()"
                        });

                        index = *middleware_close;
                        continue;
                    }
                }
            }
        }

        index = *close;
    }

    return result;
}

} // namespace gungnir::language
