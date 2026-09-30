#include <gungnir/language/transpiler.hpp>

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
    const auto tokens = Lexer{source}.tokenize();
    Parser parser{tokens, source_name};
    auto parsed = parser.parse();
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
            parsed.diagnostics.push_back(Diagnostic{
                DiagnosticLevel::error,
                SourceLocation{source_name, 1, 1},
                "Internal transpiler edit overlap"
            });
            continue;
        }

        output.append(source.substr(cursor, edit.begin - cursor));
        output += edit.replacement;
        cursor = edit.end;
    }

    output.append(source.substr(cursor));

    return TranspileResult{
        std::move(output),
        std::move(parsed.diagnostics)
    };
}

} // namespace gungnir::language
