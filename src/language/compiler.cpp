#include <gungnir/language/compiler.hpp>
#include <gungnir/language/cpp_ir.hpp>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace gungnir::language {
bool CompilationResult::success() const noexcept { return validated.has_value() && diagnostics.empty(); }
namespace {
using SourceCatalog = std::unordered_map<std::string, std::string>;

SourceLocation source_location_at(
    std::string_view source,
    std::string_view file,
    std::size_t offset
) {
    SourceLocation location{std::string{file}, 1, 1};
    offset = std::min(offset, source.size());
    for (std::size_t i = 0; i < offset; ++i) {
        if (source[i] == '\n') {
            ++location.line;
            location.column = 1;
        } else {
            ++location.column;
        }
    }
    return location;
}

std::size_t offset_at_location(
    std::string_view source,
    std::size_t target_line,
    std::size_t target_column
) {
    if (target_line == 0 || target_column == 0) return 0;
    std::size_t line = 1;
    std::size_t offset = 0;
    while (line < target_line && offset < source.size()) {
        if (source[offset++] == '\n') ++line;
    }
    return std::min(
        source.size(),
        offset + target_column - 1
    );
}

std::string source_line_at(
    std::string_view source,
    std::size_t target_line
) {
    if (target_line == 0) return {};
    std::size_t line = 1;
    std::size_t start = 0;
    while (line < target_line && start < source.size()) {
        const auto next = source.find('\n', start);
        if (next == std::string_view::npos) return {};
        start = next + 1;
        ++line;
    }
    if (line != target_line || start > source.size()) return {};
    const auto end = source.find('\n', start);
    auto text = source.substr(
        start,
        end == std::string_view::npos
            ? source.size() - start
            : end - start
    );
    if (!text.empty() && text.back() == '\r') text.remove_suffix(1);
    return std::string{text};
}

void normalize_diagnostics(
    std::vector<Diagnostic>& diagnostics,
    const SourceCatalog& sources
) {
    for (auto& diagnostic : diagnostics) {
        const auto found = sources.find(diagnostic.location.file);
        if (found == sources.end()) continue;

        const auto& source = found->second;
        if (!diagnostic.span.valid) {
            diagnostic.span.begin_offset = offset_at_location(
                source,
                diagnostic.location.line,
                diagnostic.location.column
            );
            diagnostic.span.end_offset = std::min(
                source.size(),
                diagnostic.span.begin_offset + 1
            );
            diagnostic.span.valid = true;
        }

        diagnostic.span.begin_offset =
            std::min(diagnostic.span.begin_offset, source.size());
        diagnostic.span.end_offset =
            std::min(
                std::max(
                    diagnostic.span.end_offset,
                    diagnostic.span.begin_offset
                ),
                source.size()
            );

        if (
            diagnostic.span.end_offset == diagnostic.span.begin_offset &&
            diagnostic.span.begin_offset < source.size()
        ) {
            ++diagnostic.span.end_offset;
        }

        const auto end = source_location_at(
            source,
            diagnostic.location.file,
            diagnostic.span.end_offset
        );
        diagnostic.span.end_line = end.line;
        diagnostic.span.end_column = end.column;
        diagnostic.source_line = source_line_at(
            source,
            diagnostic.location.line
        );
    }

    std::stable_sort(
        diagnostics.begin(),
        diagnostics.end(),
        [](const Diagnostic& left, const Diagnostic& right) {
            if (left.location.file != right.location.file)
                return left.location.file < right.location.file;
            if (left.location.line != right.location.line)
                return left.location.line < right.location.line;
            if (left.location.column != right.location.column)
                return left.location.column < right.location.column;
            if (left.code != right.code)
                return left.code < right.code;
            return left.message < right.message;
        }
    );
}

CompilationResult finish(
    SyntaxProject syntax,
    std::vector<Diagnostic> diagnostics,
    const CompilerOptions& options,
    const SourceCatalog& sources
) {
    CompilationResult result; result.diagnostics = std::move(diagnostics);
    if (!result.diagnostics.empty()) {
        normalize_diagnostics(result.diagnostics, sources);
        return result;
    }
    auto validation = ProgramValidator{}.validate(std::move(syntax), options);
    result.diagnostics = std::move(validation.diagnostics); result.validated = std::move(validation.project);
    normalize_diagnostics(result.diagnostics, sources);
    if (result.validated && !options.validate_only) {
        const auto ir = CppIrLowerer{}.lower(
            *result.validated,
            options.emit_line_directives
        );
        const auto verification =
            CppIrVerifier{}.verify(ir);
        if (!verification.success()) {
            std::string message{
                "Gungnir internal compiler error: invalid C++ IR"
            };
            for (const auto& error : verification.errors) {
                message += "\n - " + error;
            }
            throw std::logic_error(message);
        }
        result.code = CppEmitter{}.emit(ir);
    }
    return result;
}
void append(SyntaxProject& target, SyntaxProject source) {
    const auto m = target.modules.size(), d = target.declarations.size(), e = target.expressions.size(), s = target.statements.size();
    auto expression = [&](SyntaxId& id) { if (id != invalid_id) id += e; };
    auto statements = [&](auto& ids) { for (auto& id : ids) id += s; };
    const auto r = target.routes.size();
    for (auto& module : source.modules) {
        for (auto& id : module.declarations) id += d;
        for (auto& id : module.routes) id += r;
    }
    const auto remap_route = [&](auto&& self, RouteSyntax& route) -> void {
        route.module += m;
        for (auto& id : route.arguments) expression(id);
        for (auto& modifier : route.modifiers) for (auto& id : modifier.arguments) expression(id);
        for (auto& child : route.children) self(self, child);
    };
    for (auto& route : source.routes) remap_route(remap_route, route);
    for (auto& declaration : source.declarations) {
        declaration.module += m;
        for (auto& field : declaration.fields) expression(field.initializer);
        for (auto& metadata : declaration.metadata) expression(metadata.value);
        for (auto& relation : declaration.relationships) for (auto& argument : relation.arguments) expression(argument);
        for (auto& callable : declaration.methods) { statements(callable.body); for (auto& parameter : callable.parameters) expression(parameter.default_value); }
    }
    for (auto& node : source.expressions) { for (auto& id : node.operands) expression(id); statements(node.body); for (auto& parameter : node.parameters) expression(parameter.default_value); }
    for (auto& node : source.statements) { expression(node.expression); statements(node.body); statements(node.alternative); statements(node.parts); }
    target.modules.insert(target.modules.end(),std::make_move_iterator(source.modules.begin()),std::make_move_iterator(source.modules.end()));
    target.declarations.insert(target.declarations.end(),std::make_move_iterator(source.declarations.begin()),std::make_move_iterator(source.declarations.end()));
    target.expressions.insert(target.expressions.end(),std::make_move_iterator(source.expressions.begin()),std::make_move_iterator(source.expressions.end()));
    target.statements.insert(target.statements.end(),std::make_move_iterator(source.statements.begin()),std::make_move_iterator(source.statements.end()));
    target.routes.insert(target.routes.end(),std::make_move_iterator(source.routes.begin()),std::make_move_iterator(source.routes.end()));
}
}
CompilationResult Compiler::compile_sources(std::vector<SourceFile> files, const CompilerOptions& options) const {
    std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){ return a.file < b.file; });
    SyntaxProject project; std::vector<Diagnostic> diagnostics; SourceCatalog sources;
    for (const auto& file : files) {
        sources[file.file] = file.source;
        auto parsed = SyntaxParser{}.parse(file.source,file.file,file.module);
        diagnostics.insert(diagnostics.end(),parsed.diagnostics.begin(),parsed.diagnostics.end());
        append(project,std::move(parsed.project));
    }
    return finish(std::move(project),std::move(diagnostics),options,sources);
}
CompilationResult Compiler::compile(std::string_view source, std::string file, const CompilerOptions& options) const {
    const std::string owned_source{source};
    SourceCatalog sources{{file, owned_source}};
    auto parsed = SyntaxParser{}.parse(owned_source,file);
    return finish(std::move(parsed.project),std::move(parsed.diagnostics),options,sources);
}
CompilationResult Compiler::compile_project(const std::filesystem::path& root, const CompilerOptions& options) const {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::is_directory(root)) return {{},{{DiagnosticLevel::error,{root.string()},"Project root is not a directory","GNR2100",{}}},{}};
    for (auto it = std::filesystem::recursive_directory_iterator(root); it != std::filesystem::recursive_directory_iterator{}; ++it) {
        if (it->is_directory() && (it->path().filename() == ".git" || it->path().filename() == ".gungnir" || it->path().filename() == "build" || it->path().filename() == "vendor")) { it.disable_recursion_pending(); continue; }
        if (it->is_regular_file() && it->path().extension() == ".gnr") files.push_back(std::filesystem::absolute(it->path()));
    }
    return compile_files(root,std::move(files),options);
}
CompilationResult Compiler::compile_files(const std::filesystem::path& root, std::vector<std::filesystem::path> files, const CompilerOptions& options) const {
    std::sort(files.begin(),files.end()); files.erase(std::unique(files.begin(),files.end()),files.end());
    SyntaxProject project; std::vector<Diagnostic> diagnostics; SourceCatalog sources;
    for (auto path : files) {
        if (path.is_relative()) path = root / path;
        auto relative = std::filesystem::absolute(path).lexically_relative(std::filesystem::absolute(root)); relative.replace_extension();
        std::string module = relative.generic_string(); std::replace(module.begin(),module.end(),'/','.');
        std::ifstream input(path,std::ios::binary);
        if (!input) { diagnostics.push_back({DiagnosticLevel::error,{path.string()},"Unable to read module","GNR2100",{}}); continue; }
        const std::string source{std::istreambuf_iterator<char>{input},{}};
        const auto source_name = path.generic_string();
        sources[source_name] = source;
        auto parsed = SyntaxParser{}.parse(source,source_name,module);
        diagnostics.insert(diagnostics.end(),parsed.diagnostics.begin(),parsed.diagnostics.end()); append(project,std::move(parsed.project));
    }
    return finish(std::move(project),std::move(diagnostics),options,sources);
}
std::string dump_validated(const ValidatedProject& project) {
    std::ostringstream out;
    for (auto id : project.module_order()) out << "module " << project.syntax().modules[id].name << '\n';
    for (std::size_t i = 0; i < project.symbols().size(); ++i) { const auto& symbol = project.symbols()[i]; out << "symbol " << i << ' ' << symbol.cpp_name << " : " << project.types()[symbol.type].name << '\n'; }
    for (std::size_t i = 0; i < project.expressions().size(); ++i) out << "expression " << i << " : " << project.types()[project.expressions()[i].type].name << '\n';
    for (const auto& route : project.routes()) {
        out << "route " << project.syntax().modules[route.module].name << ' ' << route.method << ' ' << route.path << ' ' << route.name << ' ' << project.symbols()[route.controller].cpp_name << "::" << project.symbols()[route.action].cpp_name << '\n';
        for (const auto& parameter : route.parameters) out << "  parameter " << parameter << '\n';
        for (const auto& middleware : route.middleware) out << "  middleware " << (middleware.type == invalid_id ? middleware.alias : project.symbols()[middleware.type].cpp_name) << '\n';
        for (const auto& constraint : route.constraints) out << "  constraint " << constraint.parameter << ' ' << constraint.expression << '\n';
    }
    return out.str();
}
}
