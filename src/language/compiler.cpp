#include <gungnir/language/compiler.hpp>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <unordered_set>

namespace gungnir::language {
bool CompilationResult::success() const noexcept { return validated.has_value() && diagnostics.empty(); }
namespace {
CompilationResult finish(SyntaxProject syntax, std::vector<Diagnostic> diagnostics, const CompilerOptions& options) {
    CompilationResult result; result.diagnostics = std::move(diagnostics);
    if (!result.diagnostics.empty()) return result;
    auto validation = ProgramValidator{}.validate(std::move(syntax), options);
    result.diagnostics = std::move(validation.diagnostics); result.validated = std::move(validation.project);
    if (result.validated) result.code = CppEmitter{}.emit(*result.validated, options.emit_line_directives);
    return result;
}
void append(SyntaxProject& target, SyntaxProject source) {
    const auto m = target.modules.size(), d = target.declarations.size(), e = target.expressions.size(), s = target.statements.size();
    auto expression = [&](SyntaxId& id) { if (id != invalid_id) id += e; };
    auto statements = [&](auto& ids) { for (auto& id : ids) id += s; };
    for (auto& module : source.modules) for (auto& id : module.declarations) id += d;
    for (auto& declaration : source.declarations) {
        declaration.module += m;
        for (auto& field : declaration.fields) expression(field.initializer);
        for (auto& metadata : declaration.metadata) expression(metadata.value);
        for (auto& callable : declaration.methods) { statements(callable.body); for (auto& parameter : callable.parameters) expression(parameter.default_value); }
    }
    for (auto& node : source.expressions) { for (auto& id : node.operands) expression(id); statements(node.body); for (auto& parameter : node.parameters) expression(parameter.default_value); }
    for (auto& node : source.statements) { expression(node.expression); statements(node.body); statements(node.alternative); statements(node.parts); }
    target.modules.insert(target.modules.end(),std::make_move_iterator(source.modules.begin()),std::make_move_iterator(source.modules.end()));
    target.declarations.insert(target.declarations.end(),std::make_move_iterator(source.declarations.begin()),std::make_move_iterator(source.declarations.end()));
    target.expressions.insert(target.expressions.end(),std::make_move_iterator(source.expressions.begin()),std::make_move_iterator(source.expressions.end()));
    target.statements.insert(target.statements.end(),std::make_move_iterator(source.statements.begin()),std::make_move_iterator(source.statements.end()));
}
}
CompilationResult Compiler::compile(std::string_view source, std::string file, const CompilerOptions& options) const {
    auto parsed = SyntaxParser{}.parse(source,std::move(file)); return finish(std::move(parsed.project),std::move(parsed.diagnostics),options);
}
CompilationResult Compiler::compile_project(const std::filesystem::path& root, const CompilerOptions& options) const {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::is_directory(root)) return {{},{{DiagnosticLevel::error,{root.string()},"Project root is not a directory","GNR2100",{}}},{}};
    for (auto it = std::filesystem::recursive_directory_iterator(root); it != std::filesystem::recursive_directory_iterator{}; ++it) {
        if (it->is_directory() && (it->path().filename() == ".git" || it->path().filename() == "build" || it->path().filename() == "vendor")) { it.disable_recursion_pending(); continue; }
        if (it->is_regular_file() && it->path().extension() == ".gnr") files.push_back(std::filesystem::absolute(it->path()));
    }
    return compile_files(root,std::move(files),options);
}
CompilationResult Compiler::compile_files(const std::filesystem::path& root, std::vector<std::filesystem::path> files, const CompilerOptions& options) const {
    std::sort(files.begin(),files.end()); files.erase(std::unique(files.begin(),files.end()),files.end());
    SyntaxProject project; std::vector<Diagnostic> diagnostics;
    for (auto path : files) {
        if (path.is_relative()) path = root / path;
        auto relative = std::filesystem::absolute(path).lexically_relative(std::filesystem::absolute(root)); relative.replace_extension();
        std::string module = relative.generic_string(); std::replace(module.begin(),module.end(),'/','.');
        std::ifstream input(path,std::ios::binary);
        if (!input) { diagnostics.push_back({DiagnosticLevel::error,{path.string()},"Unable to read module","GNR2100",{}}); continue; }
        const std::string source{std::istreambuf_iterator<char>{input},{}};
        auto parsed = SyntaxParser{}.parse(source,path.generic_string(),module);
        diagnostics.insert(diagnostics.end(),parsed.diagnostics.begin(),parsed.diagnostics.end()); append(project,std::move(parsed.project));
    }
    return finish(std::move(project),std::move(diagnostics),options);
}
std::string dump_validated(const ValidatedProject& project) {
    std::ostringstream out;
    for (auto id : project.module_order()) out << "module " << project.syntax().modules[id].name << '\n';
    for (std::size_t i = 0; i < project.symbols().size(); ++i) { const auto& symbol = project.symbols()[i]; out << "symbol " << i << ' ' << symbol.cpp_name << " : " << project.types()[symbol.type].name << '\n'; }
    for (std::size_t i = 0; i < project.expressions().size(); ++i) out << "expression " << i << " : " << project.types()[project.expressions()[i].type].name << '\n';
    return out.str();
}
}
