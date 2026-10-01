#include <gungnir/language/lsp.hpp>
#include <gungnir/language/lexer.hpp>
#include <algorithm>
#include <map>
#include <functional>
#include <set>

namespace gungnir::language {
void LanguageServer::update(std::vector<SourceFile> sources) {
    sources_ = std::move(sources);
    CompilerOptions options; options.emit_line_directives = false;
    result_ = Compiler{}.compile_sources(sources_,options);
}
std::vector<Diagnostic> LanguageServer::diagnostics(std::string_view source,std::string file) const {
    return Compiler{}.compile(source,std::move(file)).diagnostics;
}
Origin LanguageServer::selection(const Origin& range,std::string_view name,bool last) const {
    auto result = range;
    for (const auto& source : sources_) if (source.file == range.file) {
        const auto bound = std::max(range.end,range.begin+1);
        for (const auto& token : Lexer{source.source}.tokenize(nullptr,source.file,true)) {
            if (token.offset < range.begin) continue;
            if (token.offset >= bound) break;
            if (token.lexeme == name) { result = {range.file,token.line,token.column,token.offset,token.offset+token.lexeme.size()}; if (!last) break; }
        }
        break;
    }
    return result;
}
std::vector<DocumentSymbol> LanguageServer::symbols(std::string_view file) const {
    std::vector<DocumentSymbol> result;
    for (const auto& source : sources_) if (source.file == file) {
        const auto parsed = SyntaxParser{}.parse(source.source,source.file,source.module);
        for (const auto& decl : parsed.project.declarations) {
            DocumentSymbol symbol{decl.name,decl.kind == DeclarationKind::function ? "function" : "declaration",decl.kind == DeclarationKind::function ? 12 : 5,decl.origin,selection(decl.origin,decl.name),{}};
            for (const auto& field : decl.fields) symbol.children.push_back({field.name,field.type.name,8,selection(field.origin,field.name),selection(field.origin,field.name),{}});
            if (decl.kind != DeclarationKind::function) for (const auto& method : decl.methods)
                symbol.children.push_back({method.name,method.result.name,6,method.origin,selection(method.origin,method.name),{}});
            result.push_back(std::move(symbol));
        }
    }
    return result;
}
std::optional<Origin> LanguageServer::definition(SymbolId id) const {
    if (!result_.validated || id == invalid_id) return {};
    const auto& p = *result_.validated; const auto& s = p.syntax();
    for (std::size_t d=0;d<s.declarations.size();++d) {
        const auto& decl = s.declarations[d]; const auto& resolved = p.declarations()[d];
        if (resolved.symbol == id) return selection(decl.origin,decl.name);
        for (std::size_t m=0;m<resolved.methods.size();++m) {
            if (resolved.methods[m].symbol == id) return selection(decl.methods[m].origin,decl.methods[m].name);
            for (std::size_t a=0;a<resolved.methods[m].parameters.size();++a)
                if (resolved.methods[m].parameters[a] == id) return selection(decl.methods[m].parameters[a].origin,decl.methods[m].parameters[a].name);
        }
        for (std::size_t f=0;f<resolved.fields.size();++f) if (resolved.fields[f] == id) {
            const auto& name = p.symbols()[id].name;
            for (const auto& field : decl.fields) if (field.name == name) return selection(field.origin,name);
        }
    }
    for (std::size_t i=0;i<p.bindings().size();++i) if (p.bindings()[i] == id) return selection(s.statements[i].origin,s.statements[i].name);
    for (std::size_t i=0;i<p.expressions().size();++i) for (std::size_t a=0;a<p.expressions()[i].parameters.size();++a)
        if (p.expressions()[i].parameters[a] == id) return selection(s.expressions[i].parameters[a].origin,s.expressions[i].parameters[a].name);
    // Calls of constructors and function-valued bindings carry a synthetic
    // callable; follow its declared owner, preserving lexical shadowing.
    if (id < p.symbols().size() && p.symbols()[id].owner != invalid_id) return definition(p.symbols()[id].owner);
    return {};
}
std::string LanguageServer::detail(SymbolId id) const {
    const auto& p = *result_.validated; const auto& symbol = p.symbols().at(id);
    auto type = [&](TypeId type_id) { return p.types().at(type_id).name; };
    std::string result = symbol.asynchronous ? "async " : "";
    result += type(symbol.type) + " " + symbol.name;
    if (symbol.kind == ResolvedSymbolKind::callable || symbol.kind == ResolvedSymbolKind::builtin) {
        result += '(';
        for (std::size_t a=0;a<symbol.parameters.size();++a) { if (a) result += ", "; result += type(symbol.parameters[a]); if (a < symbol.parameter_names.size()) result += " " + symbol.parameter_names[a]; }
        result += ')';
    }
    return result;
}
std::optional<SymbolInfo> LanguageServer::symbol_at(std::string_view file,std::size_t offset) const {
    if (!result_.validated) return {};
    const auto& p = *result_.validated;
    SymbolId chosen = invalid_id; Origin where; std::size_t width = invalid_id;
    for (std::size_t i=0;i<p.expressions().size();++i) {
        const auto& expr = p.syntax().expressions[i]; const auto id = p.expressions()[i].symbol;
        if (id == invalid_id || expr.origin.file != file || (expr.kind != SyntaxExpressionKind::name && expr.kind != SyntaxExpressionKind::member)) continue;
        const auto span = selection(expr.origin,expr.text,expr.kind == SyntaxExpressionKind::member);
        if (offset >= span.begin && offset < span.end && span.end-span.begin < width) { chosen=id; where=span; width=span.end-span.begin; }
    }
    if (chosen == invalid_id) for (std::size_t id=0;id<p.symbols().size();++id) {
        const auto span = definition(id);
        if (span && span->file == file && offset >= span->begin && offset < span->end) { chosen=id; where=*span; break; }
    }
    if (chosen == invalid_id) return {};
    const auto target = definition(chosen);
    return SymbolInfo{p.symbols()[chosen].name,detail(chosen),target.value_or(Origin{}),where};
}
std::vector<Completion> LanguageServer::completions(std::string_view file,std::size_t offset) const {
    std::map<std::string,Completion> entries;
    std::set<std::string> visible;
    std::string module;
    for (const auto& source : sources_) if (source.file == file) {
        const auto parsed = SyntaxParser{}.parse(source.source,source.file,source.module);
        module = source.module; visible.insert(module);
        for (const auto& m : parsed.project.modules) for (const auto& import : m.imports) visible.insert(import.module);
    }
    for (const auto& source : sources_) if (visible.contains(source.module)) {
        const auto parsed = SyntaxParser{}.parse(source.source,source.file,source.module);
        for (const auto& decl : parsed.project.declarations) {
            entries[decl.name] = {decl.name,decl.kind == DeclarationKind::function ? "function" : "declaration",decl.kind == DeclarationKind::function ? 3 : 7};
            if (source.file == file) for (const auto& method : decl.methods) if (offset >= method.origin.begin && offset <= method.origin.end)
                for (const auto& parameter : method.parameters) entries[parameter.name] = {parameter.name,parameter.type.name,6};
        }
    }
    if (result_.validated) {
        const auto& project = *result_.validated;
        const auto& syntax = project.syntax();
        std::function<void(const std::vector<SyntaxId>&)> locals = [&](const auto& statements) {
            for (auto id : statements) {
                const auto& statement = syntax.statements[id];
                if (statement.origin.begin >= offset) break;
                if (statement.kind == SyntaxStatementKind::binding && statement.origin.end <= offset) {
                    const auto symbol = project.bindings()[id];
                    if (symbol != invalid_id) entries[statement.name] = {statement.name,detail(symbol),6};
                }
                if (offset < statement.origin.end) {
                    locals(statement.parts);
                    if (!statement.alternative.empty() && offset >= syntax.statements[statement.alternative.front()].origin.begin) locals(statement.alternative);
                    else locals(statement.body);
                }
            }
        };
        for (std::size_t d=0;d<syntax.declarations.size();++d) {
            const auto& declaration = syntax.declarations[d];
            if (declaration.origin.file != file) continue;
            for (const auto& method : declaration.methods) if (offset >= method.origin.begin && offset <= method.origin.end) {
                for (auto field : project.declarations()[d].fields) entries[project.symbols()[field].name] = {project.symbols()[field].name,detail(field),5};
                locals(method.body);
            }
        }
    }
    for (const auto* keyword : {"function","controller","model","migration","middleware","event","listener","job","policy","notification","mail","import","const","let","return","if","for","async","await"}) entries.try_emplace(keyword,Completion{keyword,"keyword",14});
    std::vector<Completion> result; for (const auto& [_,entry] : entries) result.push_back(entry); return result;
}
}
