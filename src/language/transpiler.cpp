#include <gungnir/language/transpiler.hpp>
#include <gungnir/language/compiler.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <iterator>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <gungnir/language/ast.hpp>
#include <gungnir/language/async_lowering.hpp>
#include <gungnir/language/controller_lowering.hpp>
#include <gungnir/language/lexer.hpp>
#include <gungnir/language/middleware_lowering.hpp>
#include <gungnir/language/migration_lowering.hpp>
#include <gungnir/language/model_lowering.hpp>
#include <gungnir/language/parser.hpp>
#include <gungnir/language/semantic.hpp>
#include <gungnir/language/type_system.hpp>
#include <gungnir/language/validation_lowering.hpp>
#include <gungnir/language/view_lowering.hpp>

namespace gungnir::language {

namespace {

std::string escape_line_file(std::string value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (const char character : value) {
        if (character == '\\' || character == '"') {
            escaped.push_back('\\');
        }

        escaped.push_back(character);
    }

    return escaped;
}

std::string framework_base(FrameworkBaseKind kind, std::string_view name) {
    switch (kind) {
    case FrameworkBaseKind::model:
        return "public gungnir::Model<" + std::string{name} + ">";
    case FrameworkBaseKind::controller:
        return "public gungnir::Controller";
    case FrameworkBaseKind::migration:
        return "public gungnir::Migration";
    case FrameworkBaseKind::middleware:
        return "public gungnir::Middleware";
    case FrameworkBaseKind::policy:
        return "public gungnir::Policy";
    case FrameworkBaseKind::event:
        return "public gungnir::Event";
    case FrameworkBaseKind::listener:
        return "public gungnir::Listener";
    case FrameworkBaseKind::notification:
        return "public gungnir::Notification";
    case FrameworkBaseKind::mail:
        return "public gungnir::Mail";
    }

    return {};
}

} // namespace

bool TranspileResult::success() const noexcept {
    return std::none_of(
        diagnostics.begin(),
        diagnostics.end(),
        [](const Diagnostic& diagnostic) {
            return diagnostic.level == DiagnosticLevel::error;
        }
    );
}

TranspileResult Transpiler::transpile(
    std::string_view source,
    std::string source_name,
    TranspileOptions options
) const {
    if (options.structured_frontend) {
        CompilerOptions compiler_options; compiler_options.emit_line_directives = options.emit_line_directives;
        auto result = Compiler{}.compile(source,source_name,compiler_options);
        return {std::move(result.code),std::move(result.diagnostics)};
    }
    std::vector<Diagnostic> lexical_diagnostics;
    const auto tokens = Lexer{source}.tokenize(&lexical_diagnostics, source_name);
    if (!lexical_diagnostics.empty()) return {{}, std::move(lexical_diagnostics)};
    const auto unsupported = [&](const Token& token, std::string message) {
        lexical_diagnostics.push_back(Diagnostic{DiagnosticLevel::error,
            {source_name, token.line, token.column}, std::move(message), "GNR1330", {}});
    };
    std::vector<const Token*> significant;
    for (const auto& token : tokens)
        if (!token.trivia() && token.kind != TokenKind::end) significant.push_back(&token);
    std::size_t depth = 0;
    for (std::size_t i = 0; i < significant.size(); ++i) {
        const auto& token = *significant[i];
        if (depth == 0 && (token.lexeme == "module" || token.lexeme == "import" ||
                           token.lexeme == "function"))
            unsupported(token, "This declaration syntax is not implemented: " + token.lexeme);
        if (i + 1 < significant.size() && token.lexeme == "=" &&
            significant[i + 1]->lexeme == ">")
            unsupported(token, "Arrow callbacks are not implemented; use a native C++ lambda");
        if (token.lexeme == "{") ++depth;
        if (token.lexeme == "}" && depth) --depth;
    }
    if (!lexical_diagnostics.empty()) return {{}, std::move(lexical_diagnostics)};
    Parser parser{tokens, source_name};
    auto parsed = parser.parse();
    for (const auto& node : parsed.program.nodes) {
        const auto* declaration = std::get_if<FrameworkDeclaration>(&node);
        if (!declaration) continue;
        std::size_t nesting = 0;
        bool start = true;
        for (std::size_t i = 0; i < significant.size(); ++i) {
            const auto& token = *significant[i];
            if (token.offset < declaration->body_open_span.end ||
                token.offset >= declaration->body_end_span.begin) continue;
            if (nesting == 0 && i + 1 < significant.size()) {
                const auto& next = *significant[i + 1];
                if ((token.lexeme == "public" || token.lexeme == "private" ||
                     token.lexeme == "protected") && next.lexeme != ":")
                    unsupported(token, "Access modifiers require a colon in the current compiler");
                const bool relation = std::any_of(declaration->members.begin(),
                    declaration->members.end(), [&](std::size_t member) {
                        const auto* value = std::get_if<ModelRelationship>(&parsed.program.nodes[member]);
                        return value && token.offset >= value->span.begin && token.offset < value->span.end;
                    });
                if (start && token.word() && next.lexeme == "(" && !relation &&
                    token.lexeme != declaration->class_name)
                    unsupported(token, "Methods require an explicit return type in the current compiler");
                if (declaration->kind == FrameworkBaseKind::model && next.lexeme == "=" &&
                    (token.lexeme == "fillable" || token.lexeme == "hidden" ||
                     token.lexeme == "visible" || token.lexeme == "casts"))
                    unsupported(token, "This model metadata syntax is not implemented: " + token.lexeme);
                if (declaration->kind == FrameworkBaseKind::event &&
                    token.lexeme == "string" && i + 2 < significant.size() &&
                    significant[i + 2]->lexeme == ";")
                    unsupported(token, "Data-only event lowering is not implemented; use a native event class");
                if (!token.trivia()) start = false;
                if (token.lexeme == ";" || token.lexeme == ":") start = true;
            }
            if (token.lexeme == "{") ++nesting;
            if (token.lexeme == "}" && nesting) {
                --nesting;
                if (!nesting) start = true;
            }
        }
    }
    parsed.diagnostics.insert(parsed.diagnostics.end(), lexical_diagnostics.begin(),
                              lexical_diagnostics.end());
    auto semantic_diagnostics = SemanticAnalyzer{}.analyze(
        parsed.program, source_name, options.semantic_index
    );
    parsed.diagnostics.insert(parsed.diagnostics.end(),
        semantic_diagnostics.begin(), semantic_diagnostics.end());

    ModelLowerer model_lowerer;
    auto model_lowering =
        model_lowerer.lower(source, parsed.program, tokens, source_name);

    ControllerLowerer controller_lowerer;
    auto controller_lowering =
        controller_lowerer.lower(source, parsed.program, tokens, source_name);

    AsyncLowerer async_lowerer;
    auto async_lowering =
        async_lowerer.lower(tokens, parsed.program, source_name);

    ViewLowerer view_lowerer;
    auto view_lowering =
        view_lowerer.lower(source, parsed.program, tokens, source_name);

    MiddlewareLowerer middleware_lowerer;
    auto middleware_lowering =
        middleware_lowerer.lower(
            source,
            parsed.program,
            tokens,
            source_name
        );

    MigrationLowerer migration_lowerer;
    auto migration_lowering =
        migration_lowerer.lower(
            source,
            parsed.program,
            tokens,
            source_name
        );

    ValidationLowerer validation_lowerer;
    auto validation_lowering =
        validation_lowerer.lower(source, tokens, parsed.program, source_name);

    parsed.diagnostics.insert(
        parsed.diagnostics.end(),
        model_lowering.diagnostics.begin(),
        model_lowering.diagnostics.end()
    );

    parsed.diagnostics.insert(
        parsed.diagnostics.end(),
        controller_lowering.diagnostics.begin(),
        controller_lowering.diagnostics.end()
    );

    parsed.diagnostics.insert(
        parsed.diagnostics.end(),
        async_lowering.diagnostics.begin(),
        async_lowering.diagnostics.end()
    );

    parsed.diagnostics.insert(
        parsed.diagnostics.end(),
        view_lowering.diagnostics.begin(),
        view_lowering.diagnostics.end()
    );

    parsed.diagnostics.insert(
        parsed.diagnostics.end(),
        middleware_lowering.diagnostics.begin(),
        middleware_lowering.diagnostics.end()
    );

    parsed.diagnostics.insert(
        parsed.diagnostics.end(),
        migration_lowering.diagnostics.begin(),
        migration_lowering.diagnostics.end()
    );

    parsed.diagnostics.insert(
        parsed.diagnostics.end(),
        validation_lowering.diagnostics.begin(),
        validation_lowering.diagnostics.end()
    );

    std::vector<SourceEdit> edits;
    edits.reserve(
        parsed.program.nodes.size() +
        model_lowering.edits.size() +
        controller_lowering.edits.size() +
        async_lowering.edits.size() +
        view_lowering.edits.size() +
        middleware_lowering.edits.size() +
        migration_lowering.edits.size() +
        validation_lowering.edits.size()
    );

    for (const auto& node : parsed.program.nodes) {
        std::visit(
            [&](const auto& value) {
                using NodeType = std::decay_t<decltype(value)>;

                if constexpr (std::same_as<NodeType, InferredBinding>) {
                    edits.push_back(SourceEdit{
                        value.span.begin,
                        value.span.end,
                        value.immutable ? "const auto " : "auto "
                    });
                } else if constexpr (std::same_as<NodeType, ApplicationReference>) {
                    edits.push_back(SourceEdit{
                        value.span.begin, value.span.end, "gungnir::Application"
                    });
                } else if constexpr (std::same_as<NodeType, FrameworkDeclaration>) {
                    edits.push_back(SourceEdit{
                        value.keyword_span.begin, value.keyword_span.end, "class"
                    });
                    edits.push_back(SourceEdit{
                        value.name_end_span.begin, value.name_end_span.end,
                        " : " + framework_base(value.kind, value.class_name)
                    });
                    if (value.needs_semicolon &&
                        value.kind != FrameworkBaseKind::model &&
                        value.kind != FrameworkBaseKind::controller &&
                        value.kind != FrameworkBaseKind::migration &&
                        value.kind != FrameworkBaseKind::middleware) {
                        edits.push_back(SourceEdit{
                            value.body_end_span.begin, value.body_end_span.end, ";"
                        });
                    }
                    if (value.kind != FrameworkBaseKind::model &&
                        value.kind != FrameworkBaseKind::controller &&
                        value.kind != FrameworkBaseKind::migration &&
                        value.kind != FrameworkBaseKind::middleware) {
                        edits.push_back(SourceEdit{
                            value.body_open_span.end, value.body_open_span.end,
                            "\npublic:\n"
                        });
                    }
                } else if constexpr (std::same_as<NodeType, FrameworkBase>) {
                    edits.push_back(SourceEdit{
                        value.span.begin,
                        value.span.end,
                        framework_base(value.kind, value.class_name)
                    });
                    if (value.kind != FrameworkBaseKind::model &&
                        value.kind != FrameworkBaseKind::controller &&
                        value.kind != FrameworkBaseKind::migration &&
                        value.kind != FrameworkBaseKind::middleware &&
                        value.declaration_span.end > value.span.end) {
                        edits.push_back(SourceEdit{
                            value.body_open_span.end, value.body_open_span.end,
                            "\npublic:\n"
                        });
                    }
                }
            },
            node
        );
    }

    const auto list_type = [&](const auto& self, const Expression& expression)
        -> std::string {
        if (expression.kind == ExpressionKind::literal) {
            const auto type = TypeSystem{}.infer_literal(expression.text);
            return type.known() ? type.name : std::string{};
        }
        if (expression.kind == ExpressionKind::list) {
            if (expression.arguments.empty()) return "list<Value>";
            std::string element;
            for (const auto& argument : expression.arguments) {
                const auto type = self(self, argument);
                if (type.empty()) continue;
                if (element.empty()) element = type;
                else if (element != type) return "list<Value>";
            }
            return element.empty() ? std::string{} : "list<" + element + ">";
        }
        if (expression.kind == ExpressionKind::object) return "map";
        return {};
    };
    const auto lower_literals = [&](const auto& self, const Expression& expression,
                                    bool view_data)
        -> void {
        if (expression.kind == ExpressionKind::list) {
            std::string element;
            bool mixed = false;
            for (const auto& argument : expression.arguments) {
                const auto type = list_type(list_type, argument);
                if (type.empty()) continue;
                if (element.empty()) element = type;
                else if (element != type) mixed = true;
            }
            edits.push_back(SourceEdit{
                expression.span.begin, expression.span.begin + 1,
                expression.arguments.empty() || mixed
                    ? "std::vector<gungnir::view::Value>{" : "std::vector{"
            });
            edits.push_back(SourceEdit{
                expression.span.end - 1, expression.span.end, "}"
            });
            if (mixed) {
                for (const auto& argument : expression.arguments) {
                    edits.push_back(SourceEdit{argument.span.begin, argument.span.begin,
                                               "gungnir::view::make_value("});
                    edits.push_back(SourceEdit{argument.span.end, argument.span.end, ")"});
                }
            }
        }
        bool native_initializer = false;
        if (expression.kind == ExpressionKind::object &&
            expression.arguments.empty()) {
            const auto previous = std::find_if(tokens.rbegin(), tokens.rend(),
                [&](const Token& token) {
                    return !token.trivia() && token.offset < expression.span.begin;
                });
            native_initializer = previous != tokens.rend() &&
                (previous->lexeme == ">" || previous->lexeme == ")" ||
                 previous->kind == TokenKind::identifier);
        }
        if (expression.kind == ExpressionKind::object && !view_data &&
            !native_initializer) {
            edits.push_back(SourceEdit{expression.span.begin,
                                       expression.span.begin + 1,
                                       "gungnir::view::Data{"});
            for (std::size_t i = 0; i < expression.arguments.size(); ++i) {
                const auto& entry = expression.arguments[i];
                if (entry.kind != ExpressionKind::entry ||
                    entry.arguments.size() != 2) continue;
                const auto& key = entry.arguments[0];
                const auto& value = entry.arguments[1];
                if (key.kind != ExpressionKind::literal || key.text.empty() ||
                    key.text.front() != '"') {
                    parsed.diagnostics.push_back(Diagnostic{
                        DiagnosticLevel::error,
                        SourceLocation{source_name, key.span.line, key.span.column},
                        "Object keys must be string literals", "GNR1014", {}
                    });
                    continue;
                }
                edits.push_back(SourceEdit{key.span.begin, key.span.begin, "{"});
                for (const auto& token : tokens) {
                    if (token.offset < key.span.end) continue;
                    if (token.offset >= value.span.begin) break;
                    if (token.lexeme == ":") {
                        edits.push_back(SourceEdit{token.offset, token.offset + 1, ","});
                        break;
                    }
                }
                auto end = expression.span.end - 1;
                if (i + 1 < expression.arguments.size()) {
                    for (const auto& token : tokens) {
                        if (token.offset < entry.span.end) continue;
                        if (token.offset >= expression.arguments[i + 1].span.begin) break;
                        if (token.lexeme == ",") {
                            end = token.offset;
                            break;
                        }
                    }
                }
                edits.push_back(SourceEdit{end, end, "}"});
            }
        }
        bool view_call = false, validation_call = false;
        if (expression.kind == ExpressionKind::call &&
            !expression.arguments.empty()) {
            const auto& callee = expression.arguments.front();
            view_call = callee.kind == ExpressionKind::name &&
                        callee.text == "view";
            const auto* name = &callee;
            if (callee.kind == ExpressionKind::member &&
                callee.arguments.size() == 2) name = &callee.arguments[1];
            validation_call = name->kind == ExpressionKind::name &&
                              name->text == "validate";
        }
        for (std::size_t i = 0; i < expression.arguments.size(); ++i)
            self(self, expression.arguments[i],
                 (view_call && i == 2) || (validation_call && i == 1));
    };
    const auto lower_list_statements = [&](const auto& self,
                                           const std::vector<MethodStatement>& statements)
        -> void {
        for (const auto& statement : statements) {
            if (statement.for_parts.empty())
                lower_literals(lower_literals, statement.expression, false);
            for (std::size_t i = 0; i < statement.for_parts.size(); ++i) {
                if (i == 0 && statement.for_binding_immutable) continue;
                lower_literals(lower_literals, statement.for_parts[i], false);
            }
            if (statement.for_binding_immutable)
                lower_literals(lower_literals, statement.for_binding_initializer, false);
            self(self, statement.children);
            self(self, statement.alternative);
        }
    };
    for (const auto& node : parsed.program.nodes) {
        if (const auto* binding = std::get_if<InferredBinding>(&node))
            lower_literals(lower_literals, binding->initializer, false);
        if (const auto* method = std::get_if<FrameworkMethod>(&node))
            lower_list_statements(lower_list_statements, method->body);
        if (const auto* method = std::get_if<ControllerMethod>(&node))
            lower_list_statements(lower_list_statements, method->body);
    }

    const auto lower_for_initializers = [&](const auto& self,
                                             const std::vector<MethodStatement>& statements,
                                             std::unordered_set<std::string> visible)
        -> void {
        for (const auto& statement : statements) {
            auto nested = visible;
            if (statement.kind == StatementKind::loop_ && statement.name == "for" &&
                !statement.for_parts.empty() &&
                !statement.for_binding_name.empty()) {
                const auto& initializer = statement.for_parts.front();
                if (statement.for_binding_immutable) {
                    edits.push_back(SourceEdit{initializer.span.begin,
                                               initializer.span.begin + 5,
                                               "const auto"});
                } else if (!visible.contains(statement.for_binding_name)) {
                    edits.push_back(SourceEdit{initializer.span.begin,
                                               initializer.span.begin, "auto "});
                }
                nested.insert(statement.for_binding_name);
            }
            self(self, statement.children, nested);
            self(self, statement.alternative, visible);
            if (statement.kind == StatementKind::binding && !statement.name.empty())
                visible.insert(statement.name);
            if (statement.kind == StatementKind::expression &&
                statement.expression.kind == ExpressionKind::binary &&
                statement.expression.text == "=" &&
                !statement.expression.arguments.empty() &&
                statement.expression.arguments.front().kind == ExpressionKind::name)
                visible.insert(statement.expression.arguments.front().text);
            // Native C++ typed locals remain source-preserving, but their names
            // must be visible to later inferred for-loop initializers.
            if (statement.kind == StatementKind::expression) {
                std::array<const Token*, 3> words{};
                std::size_t count = 0;
                auto token = std::lower_bound(tokens.begin(), tokens.end(),
                    statement.span.begin, [](const Token& candidate, std::size_t offset) {
                        return candidate.offset < offset;
                    });
                for (; token != tokens.end() && token->offset < statement.span.end &&
                       count < words.size(); ++token) {
                    if (!token->trivia()) words[count++] = &*token;
                }
                if (count == 3 && words[0]->word() &&
                    words[1]->kind == TokenKind::identifier &&
                    (words[2]->lexeme == "=" || words[2]->lexeme == ";"))
                    visible.insert(words[1]->lexeme);
            }
        }
    };
    for (const auto& node : parsed.program.nodes) {
        const auto* framework = std::get_if<FrameworkMethod>(&node);
        const auto* controller = std::get_if<ControllerMethod>(&node);
        if (!framework && !controller) continue;
        std::unordered_set<std::string> visible;
        const auto& parameters = framework ? framework->parameters : controller->parameters;
        for (const auto& parameter : parameters) visible.insert(parameter.name);
        lower_for_initializers(lower_for_initializers,
                               framework ? framework->body : controller->body,
                               std::move(visible));
    }

    edits.insert(
        edits.end(),
        std::make_move_iterator(model_lowering.edits.begin()),
        std::make_move_iterator(model_lowering.edits.end())
    );

    edits.insert(
        edits.end(),
        std::make_move_iterator(controller_lowering.edits.begin()),
        std::make_move_iterator(controller_lowering.edits.end())
    );

    edits.insert(
        edits.end(),
        std::make_move_iterator(async_lowering.edits.begin()),
        std::make_move_iterator(async_lowering.edits.end())
    );

    edits.insert(
        edits.end(),
        std::make_move_iterator(view_lowering.edits.begin()),
        std::make_move_iterator(view_lowering.edits.end())
    );

    edits.insert(
        edits.end(),
        std::make_move_iterator(middleware_lowering.edits.begin()),
        std::make_move_iterator(middleware_lowering.edits.end())
    );

    edits.insert(
        edits.end(),
        std::make_move_iterator(migration_lowering.edits.begin()),
        std::make_move_iterator(migration_lowering.edits.end())
    );

    edits.insert(
        edits.end(),
        std::make_move_iterator(validation_lowering.edits.begin()),
        std::make_move_iterator(validation_lowering.edits.end())
    );

    std::sort(
        edits.begin(),
        edits.end(),
        [](const SourceEdit& left, const SourceEdit& right) {
            if (left.begin != right.begin) {
                return left.begin < right.begin;
            }

            return left.end < right.end;
        }
    );

    std::string output;
    output.reserve(source.size() + edits.size() * 12);

    if (options.emit_line_directives) {
        output += "#line 1 \"" + escape_line_file(source_name) + "\"\n";
    }

    std::size_t cursor = 0;

    for (const auto& edit : edits) {
        if (
            edit.begin < cursor ||
            edit.end < edit.begin ||
            edit.end > source.size()
        ) {
            SourceLocation location{source_name, 1, 1};
            for (std::size_t i = 0; i < std::min(edit.begin, source.size()); ++i) {
                if (source[i] == '\n') {
                    ++location.line;
                    location.column = 1;
                } else {
                    ++location.column;
                }
            }
            parsed.diagnostics.push_back(Diagnostic{
                DiagnosticLevel::error,
                std::move(location),
                "Internal transpiler edit overlap", "GNR1901", {}
            });
            continue;
        }

        output.append(source.substr(cursor, edit.begin - cursor));
        output += edit.replacement;
        cursor = edit.end;
    }

    output.append(source.substr(cursor));

    if (std::any_of(parsed.diagnostics.begin(), parsed.diagnostics.end(),
                    [](const Diagnostic& d) { return d.level == DiagnosticLevel::error; }))
        output.clear();
    return TranspileResult{std::move(output), std::move(parsed.diagnostics)};
}

} // namespace gungnir::language

