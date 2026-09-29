#include <gungnir/language/controller_lowering.hpp>

#include <algorithm>
#include <functional>
#include <unordered_set>

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
    return lower(source, program, Lexer{source}.tokenize(), std::move(source_name));
}

ControllerLoweringResult ControllerLowerer::lower(
    std::string_view source, const Program& program,
    const std::vector<Token>& tokens, std::string source_name
) const {
    ControllerLoweringResult result;

    (void) source_name;

    std::vector<ControllerInfo> controllers;

    for (const auto& node : program.nodes) {
        const auto* base = std::get_if<FrameworkBase>(&node);
        const auto* framework = std::get_if<FrameworkDeclaration>(&node);
        const auto kind = base ? base->kind : framework
            ? framework->kind : FrameworkBaseKind::model;
        if (kind != FrameworkBaseKind::controller || (!base && !framework)) {
            continue;
        }
        const auto& members = base ? base->members : framework->members;
        const auto& name = base ? base->class_name : framework->class_name;
        const auto span = base ? base->declaration_span : framework->span;
        const auto body = base ? base->body_open_span
            : framework->body_open_span;
        const auto body_offset = body.begin;
        if (body.end == 0 || body_offset >= span.end || span.end == 0) {
            continue;
        }
        const auto body_open = std::find_if(tokens.begin(), tokens.end(),
            [&](const Token& token) { return token.offset == body_offset; });
        const auto body_close = std::find_if(tokens.begin(), tokens.end(),
            [&](const Token& token) { return token.offset == span.end - 1; });
        if (body_open == tokens.end() || body_close == tokens.end()) continue;
        ControllerInfo controller;
        controller.name = name;
        controller.body_open_end = body_offset + 1;
        controller.body_close_offset = body_close->offset;
        controller.metadata_offset = span.end;
        const auto after = next_significant(tokens,
            static_cast<std::size_t>(body_close - tokens.begin()));
        if (after && tokens[*after].lexeme == ";") {
            controller.metadata_offset =
                tokens[*after].offset + tokens[*after].lexeme.size();
        } else {
            controller.needs_semicolon = true;
        }

        for (const auto member_index : members) {
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

        std::unordered_set<std::size_t> lowered_dots;
        std::function<void(const Expression&)> lower_expression =
            [&](const Expression& expression) {
            if (expression.kind == ExpressionKind::member &&
                expression.text == "." && expression.arguments.size() == 2 &&
                expression.arguments[0].kind == ExpressionKind::name) {
                const auto& object = expression.arguments[0];
                const auto injected = std::find_if(
                    controller.injections.begin(), controller.injections.end(),
                    [&](const Injection& value) { return value.name == object.text; }
                );
                if (injected != controller.injections.end()) {
                    const auto dot = std::find_if(tokens.begin(), tokens.end(),
                        [&](const Token& token) {
                            return token.offset >= object.span.end &&
                                   token.offset < expression.arguments[1].span.begin &&
                                   token.lexeme == ".";
                        });
                    if (dot != tokens.end() &&
                        lowered_dots.insert(dot->offset).second)
                        result.edits.push_back(SourceEdit{
                            dot->offset, dot->offset + 1, "->"
                        });
                }
            }
            for (const auto& child : expression.arguments) lower_expression(child);
        };
        std::function<void(const std::vector<MethodStatement>&)> lower_body =
            [&](const std::vector<MethodStatement>& statements) {
            for (const auto& statement : statements) {
                if (statement.for_parts.empty()) lower_expression(statement.expression);
                for (const auto& part : statement.for_parts) lower_expression(part);
                lower_body(statement.children);
                lower_body(statement.alternative);
            }
        };
        for (const auto member_index : members) {
            if (const auto* method = std::get_if<ControllerMethod>(
                    &program.nodes[member_index]))
                lower_body(method->body);
        }

        for (const auto& injection : controller.injections) {
            for (
                std::size_t cursor = static_cast<std::size_t>(body_open - tokens.begin()) + 1;
                cursor < static_cast<std::size_t>(body_close - tokens.begin());
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
                if (dot && tokens[*dot].lexeme == "." &&
                    !lowered_dots.contains(tokens[*dot].offset)) {
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
