#include <gungnir/language/controller_lowering.hpp>

#include <optional>
#include <string>
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

std::string route_method_name(RouteMethodKind method) {
    switch (method) {
    case RouteMethodKind::get:
        return "get";
    case RouteMethodKind::post:
        return "post";
    case RouteMethodKind::put:
        return "put";
    case RouteMethodKind::patch:
        return "patch";
    case RouteMethodKind::remove:
        return "remove";
    case RouteMethodKind::options:
        return "options";
    case RouteMethodKind::head:
        return "head";
    }

    return {};
}

} // namespace

ControllerLoweringResult ControllerLowerer::lower(
    std::string_view source,
    const Program& program,
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

        const FrameworkBase* declaration = nullptr;
        for (const auto& node : program.nodes) {
            const auto* candidate = std::get_if<FrameworkBase>(&node);
            if (candidate && candidate->kind == FrameworkBaseKind::controller &&
                candidate->span.begin == tokens[*base].offset) {
                declaration = candidate;
                break;
            }
        }
        if (!declaration) continue;

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

        for (const auto member_index : declaration->members) {
            const auto& node = program.nodes[member_index];
            const auto* declaration =
                std::get_if<InjectDeclaration>(&node);

            if (!declaration) {
                continue;
            }

            controller.injections.push_back(Injection{
                declaration->type_name,
                declaration->name,
                declaration->span.begin,
                declaration->span.end
            });

            result.edits.push_back(SourceEdit{
                declaration->span.begin,
                declaration->span.end,
                "std::shared_ptr<" + declaration->type_name + "> " +
                    declaration->name
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

    for (const auto& node : program.nodes) {
        const auto* route =
            std::get_if<RouteDeclaration>(&node);

        if (!route) {
            continue;
        }

        result.edits.push_back(SourceEdit{
            route->route_span.begin,
            route->route_span.end,
            "gungnir::Route"
        });

        result.edits.push_back(SourceEdit{
            route->method_span.begin,
            route->method_span.end,
            route_method_name(route->method) +
                "<" + route->controller_name + ">"
        });

        result.edits.push_back(SourceEdit{
            route->handler_prefix_span.begin,
            route->handler_prefix_span.end,
            "&"
        });

        if (route->has_middleware) {
            result.edits.push_back(SourceEdit{
                route->middleware_span.begin,
                route->middleware_span.end,
                "middleware<" +
                    route->middleware_type + ">()"
            });
        }
    }

    return result;
}

} // namespace gungnir::language
