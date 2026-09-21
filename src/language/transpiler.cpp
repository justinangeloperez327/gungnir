#include <gungnir/language/transpiler.hpp>

#include <algorithm>
#include <concepts>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include <gungnir/language/ast.hpp>
#include <gungnir/language/controller_lowering.hpp>
#include <gungnir/language/lexer.hpp>
#include <gungnir/language/model_lowering.hpp>
#include <gungnir/language/parser.hpp>

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

std::string framework_base(const FrameworkBase& node) {
    switch (node.kind) {
    case FrameworkBaseKind::model:
        return "public gungnir::Model<" + node.class_name + ">";
    case FrameworkBaseKind::controller:
        return "public gungnir::Controller";
    case FrameworkBaseKind::migration:
        return "public gungnir::Migration";
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
    Lexer lexer{source};
    Parser parser{lexer.tokenize(), source_name};
    auto parsed = parser.parse();

    ModelLowerer model_lowerer;
    auto model_lowering = model_lowerer.lower(source, source_name);

    ControllerLowerer controller_lowerer;
    auto controller_lowering =
        controller_lowerer.lower(source, source_name);

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

    std::vector<SourceEdit> edits;
    edits.reserve(
        parsed.program.nodes.size() +
        model_lowering.edits.size() +
        controller_lowering.edits.size()
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
                } else if constexpr (std::same_as<NodeType, FrameworkBase>) {
                    edits.push_back(SourceEdit{
                        value.span.begin,
                        value.span.end,
                        framework_base(value)
                    });
                }
            },
            node
        );
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
