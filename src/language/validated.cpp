#include <gungnir/language/compiler.hpp>
#include <algorithm>
#include <cctype>
#include <functional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace gungnir::language {
namespace {
std::string namespace_name(std::string_view module) {
    std::string out = "::gnr";
    for (char c : module) out += c == '.' ? "::" : std::string{c};
    return module.empty() ? "" : "::gnr::" + out.substr(5);
}
std::string snake(std::string value) {
    std::string result;
    for (const auto c : value) { if (c >= 'A' && c <= 'Z') { result += '_'; result += static_cast<char>(c + 32); } else result += c; }
    return result;
}
}
class ValidationEngine {
public:
    ValidatedProject v;
    std::vector<Diagnostic> diagnostics;
    const CompilerOptions& options;
    std::unordered_map<std::string, TypeId> type_names;
    std::unordered_map<std::string, SymbolId> declarations;
    std::unordered_map<std::string, SymbolId> members;
    std::unordered_map<std::string, std::size_t> modules;
    std::unordered_map<SymbolId, std::size_t> declaration_ids;
    std::vector<std::unordered_map<std::string, SymbolId>> scopes;
    std::vector<std::unordered_set<SymbolId>*> capture_sets;
    std::size_t current_module = 0;
    SymbolId current_owner = invalid_id;
    bool async_context = false;
    unsigned loops = 0;
    TypeId return_type = invalid_id;
    std::vector<TypeId> returned;
    std::unordered_set<SymbolId> narrowed;
    std::unordered_set<SyntaxId> awaited_calls;
    explicit ValidationEngine(SyntaxProject syntax, const CompilerOptions& options) : options(options) {
        v.syntax_ = std::move(syntax); v.expressions_.resize(v.syntax_.expressions.size());
        v.bindings_.resize(v.syntax_.statements.size(), invalid_id); v.declarations_.resize(v.syntax_.declarations.size());
        builtin_types();
    }
    void report(const Origin& origin, std::string message, std::string code = "GNR2201") {
        diagnostics.push_back({DiagnosticLevel::error, {origin.file, origin.line, origin.column}, std::move(message), std::move(code), {}});
    }
    TypeId intern(std::string name, std::string cpp, std::vector<TypeId> arguments = {}, bool optional = false) {
        std::string key = name;
        for (auto id : arguments) key += ":" + std::to_string(id);
        if (optional) key += "?";
        if (auto found = type_names.find(key); found != type_names.end()) return found->second;
        const auto id = v.types_.size(); type_names.emplace(std::move(key), id); v.types_.push_back({std::move(name), std::move(cpp), std::move(arguments), optional}); return id;
    }
    TypeId type_id(std::string_view name) const { auto found = type_names.find(std::string{name}); return found == type_names.end() ? invalid_id : found->second; }
    const ResolvedType& type(TypeId id) const { return v.types_.at(id); }
    TypeId optional(TypeId id) { const auto t = type(id); return t.optional ? id : intern(t.name, "std::optional<" + t.cpp_name + ">", t.arguments, true); }
    TypeId unoptional(TypeId id) { const auto t = type(id); if (!t.optional) return id; return intern(t.name, t.cpp_name.substr(14, t.cpp_name.size() - 15), t.arguments); }
    TypeId sequence(std::string name, TypeId element) {
        return intern(name, (name == "Query" ? "gungnir::orm::Query<" : name == "Collection" ? "gungnir::orm::Collection<" : "std::vector<") + type(element).cpp_name + ">", {element});
    }
    bool valid_identifier(std::string_view name) const {
        if (name.empty() || !(std::isalpha(static_cast<unsigned char>(name.front())) || name.front() == '_')) return false;
        for (auto c : name) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
        static const std::unordered_set<std::string> reserved{"alignas","alignof","and","asm","auto","bitand","bitor","bool","case","catch","char","class","compl","concept","const","consteval","constexpr","constinit","continue","co_await","co_return","co_yield","decltype","default","delete","do","double","else","enum","explicit","export","extern","false","float","for","friend","goto","if","inline","int","long","mutable","namespace","new","noexcept","not","nullptr","operator","or","private","protected","public","register","requires","return","short","signed","sizeof","static","struct","switch","template","this","thread_local","throw","true","try","typedef","typeid","typename","union","unsigned","using","virtual","void","volatile","while","xor"};
        return !reserved.contains(std::string{name}) && !name.starts_with("gnr_");
    }
    SymbolId symbol(ResolvedSymbol value) { v.symbols_.push_back(std::move(value)); return v.symbols_.size() - 1; }
    void builtin_types() {
        for (const auto& [name, cpp] : std::vector<std::pair<std::string,std::string>>{
            {"void","void"},{"int","gungnir::Int64"},{"uint64","gungnir::UInt64"},{"double","double"},{"decimal","double"},{"Decimal","gungnir::model::Decimal"},{"bool","bool"},{"string","gungnir::String"},
            {"null","std::nullptr_t"},{"Value","gungnir::Json"},{"Json","gungnir::Json"},{"Data","gungnir::Json"},{"Response","gungnir::Response"},{"Request","gungnir::Request"},
            {"Next","gungnir::Next"},{"Decision","gungnir::auth::Decision"},{"Table","gungnir::migration::Table"},{"Column","gungnir::migration::Column"},
            {"ColumnDefinition","gungnir::migration::ColumnDefinition"},{"IndexDefinition","gungnir::migration::IndexDefinition"},{"ForeignKeyDefinition","gungnir::migration::ForeignKeyDefinition"},
            {"Callable","auto"},{"inferred","auto"}}) intern(name, cpp);
        for (const auto& native : options.native_types) {
            if (type_id(native.name) != invalid_id) report({}, "Native type conflicts with a prelude type: " + native.name);
            else intern(native.name, native.cpp_name);
        }
    }
    SymbolId visible(std::string name, const Origin& where, bool diagnose = true) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) if (const auto found = it->find(name); found != it->end()) {
            if (!capture_sets.empty()) capture_sets.back()->insert(found->second);
            return found->second;
        }
        const auto own = declarations.find(std::to_string(current_module) + ":" + name);
        if (own != declarations.end()) return own->second;
        const auto split = name.find("::");
        if (split != std::string::npos) {
            const auto alias = name.substr(0, split);
            for (const auto& import : v.syntax_.modules[current_module].imports)
                if (import.alias == alias && modules.contains(import.module)) {
                    const auto found = declarations.find(std::to_string(modules.at(import.module)) + ":" + name.substr(split + 2));
                    if (found != declarations.end()) return found->second;
                }
        }
        SymbolId selected = invalid_id;
        for (const auto& import : v.syntax_.modules[current_module].imports) {
            if (!import.alias.empty() || !modules.contains(import.module)) continue;
            const auto found = declarations.find(std::to_string(modules.at(import.module)) + ":" + name);
            if (found == declarations.end()) continue;
            if (selected != invalid_id && selected != found->second) { if (diagnose) report(where, "Ambiguous imported name '" + name + "'", "GNR2105"); return invalid_id; }
            selected = found->second;
        }
        if (selected != invalid_id) return selected;
        if (current_owner != invalid_id) {
            const auto found = members.find(std::to_string(current_owner) + ":" + name);
            if (found != members.end()) { if (!capture_sets.empty()) capture_sets.back()->insert(found->second); return found->second; }
        }
        if (const auto found = declarations.find("native:" + name); found != declarations.end()) return found->second;
        if (type_id(name) != invalid_id) {
            const auto id = symbol({ResolvedSymbolKind::builtin, name, type(type_id(name)).cpp_name, type_id(name)});
            declarations.emplace("native:" + name, id); return id;
        }
        if (diagnose) report(where, "Unknown name '" + name + "'", "GNR2202"); return invalid_id;
    }
    TypeId resolve(const TypeSyntax& syntax) {
        std::string name = syntax.name;
        if (name == "integer" || name == "int64" || name == "Integer" || name == "Int64") name = "int";
        if (name == "boolean" || name == "Boolean") name = "bool";
        if (name == "String") name = "string";
        if (name == "float" || name == "Double") name = "double";
        if (name == "json" || name == "map") name = "Json";
        if (name == "list") name = "List";
        TypeId value = invalid_id;
        if (name == "List" || name == "Collection" || name == "Query") {
            if (syntax.arguments.size() != 1) { report(syntax.origin, name + " requires one type argument"); return type_id("Value"); }
            value = sequence(name, resolve(syntax.arguments.front()));
        } else if (name == "Map") {
            if (syntax.arguments.size() != 2 || syntax.arguments[0].name != "string") { report(syntax.origin, "Map requires string keys and one value type"); return type_id("Value"); }
            const auto element = resolve(syntax.arguments[1]); value = intern("Map", "std::unordered_map<gungnir::String," + type(element).cpp_name + ">", {type_id("string"), element});
        } else if (name == "Optional") {
            if (syntax.arguments.size() != 1) { report(syntax.origin, "Optional requires one type argument"); return type_id("Value"); }
            value = optional(resolve(syntax.arguments[0]));
        } else {
            value = type_id(name);
            if (value == invalid_id) { auto id = visible(name, syntax.origin, false); if (id != invalid_id) value = v.symbols_[id].type; }
            if (value == invalid_id) { report(syntax.origin, "Unknown type '" + name + "'", "GNR2203"); value = type_id("Value"); }
            if (!syntax.arguments.empty()) report(syntax.origin, "This type does not accept type arguments: " + name);
        }
        if ((syntax.optional || !syntax.arguments.empty()) &&
            (value == type_id("void") || std::any_of(type(value).arguments.begin(), type(value).arguments.end(), [&](auto id) { return id == type_id("void"); })))
            report(syntax.origin, "void cannot be used as an optional or collection element");
        return syntax.optional ? optional(value) : value;
    }
    bool numeric(TypeId id) const { const auto& name = type(id).name; return !type(id).optional && (name == "int" || name == "uint64" || name == "double" || name == "decimal"); }
    bool assignable(TypeId target, TypeId source) {
        if (target == source || type(target).name == "Value") return true;
        const auto a = type(target), b = type(source);
        if (a.optional) return b.name == "null" || assignable(unoptional(target), b.optional ? unoptional(source) : source);
        if (b.optional) return false;
        if ((a.name == "double" || a.name == "decimal") && numeric(source)) return true;
        if (a.name == "Decision" && b.name == "bool") return true;
        if ((a.name == "Json" || a.name == "Data") && b.name != "void" && b.name != "Callable") return true;
        if (a.name == "List" && b.name == "List" && a.arguments.size() == 1 && b.arguments.size() == 1) return a.arguments[0] == b.arguments[0];
        return false;
    }
    void bind(std::string name, SymbolId id, const Origin& origin) {
        if (!valid_identifier(name)) report(origin,"Binding name conflicts with a reserved native identifier");
        if (!scopes.back().emplace(name, id).second) report(origin, "Duplicate binding '" + name + "'", "GNR2204");
    }
    SymbolId owner_of(TypeId id) {
        const auto base = unoptional(id);
        for (const auto& [symbol, declaration] : declaration_ids) if (v.symbols_[symbol].type == base) return symbol;
        return invalid_id;
    }
    SymbolId member(TypeId receiver, std::string name, const Origin& origin, bool diagnose = true) {
        const auto owner = owner_of(receiver);
        if (owner != invalid_id) {
            const auto found = members.find(std::to_string(owner) + ":" + name);
            if (found != members.end()) {
                if (v.symbols_[found->second].visibility != Visibility::public_ && owner != current_owner) report(origin, "Member is not public: " + name, "GNR2205");
                return found->second;
            }
        }
        for (const auto& field : options.native_fields) if (type(receiver).name == field.owner && name == field.name) {
            const auto id = symbol({ResolvedSymbolKind::field, name, name, resolve({field.type, {}, false, origin}), owner});
            return id;
        }
        if (diagnose) report(origin, "Unknown member '" + name + "' on " + type(receiver).name, "GNR2206"); return invalid_id;
    }
    SymbolId builtin(std::string name, std::string cpp, TypeId result, std::vector<TypeId> parameters = {}, bool async = false) {
        ResolvedSymbol value{ResolvedSymbolKind::builtin, std::move(name), std::move(cpp), result}; value.parameters = std::move(parameters); value.asynchronous = async; return symbol(std::move(value));
    }
    void index() {
        for (std::size_t m = 0; m < v.syntax_.modules.size(); ++m) {
            const auto& module = v.syntax_.modules[m];
            std::size_t component = 0; while (component < module.name.size()) { const auto end = module.name.find('.',component); if (!valid_identifier(module.name.substr(component,end == std::string::npos ? end : end-component))) report(module.origin,"Module component conflicts with a reserved native identifier"); if (end == std::string::npos) break; component = end+1; }
            if (!modules.emplace(module.name, m).second) report(module.origin, "Duplicate module '" + module.name + "'", "GNR2102");
            std::unordered_set<std::string> aliases;
            for (const auto& import : module.imports) if (!import.alias.empty() && !aliases.insert(import.alias).second) report(import.origin, "Duplicate import alias '" + import.alias + "'", "GNR2103");
        }
        std::vector<unsigned> state(v.syntax_.modules.size());
        std::function<void(std::size_t)> visit = [&](std::size_t m) {
            if (state[m] == 2) return;
            if (state[m] == 1) { report(v.syntax_.modules[m].origin, "Circular module dependency", "GNR2104"); return; }
            state[m] = 1;
            for (const auto& import : v.syntax_.modules[m].imports) {
                const auto found = modules.find(import.module);
                if (found == modules.end()) report(import.origin, "Module not found: " + import.module, "GNR2106"); else visit(found->second);
            }
            state[m] = 2; v.module_order_.push_back(m);
        };
        std::vector<std::pair<std::string,std::size_t>> ordered(modules.begin(), modules.end()); std::sort(ordered.begin(), ordered.end()); for (const auto& [_, m] : ordered) visit(m);
        for (std::size_t d = 0; d < v.syntax_.declarations.size(); ++d) {
            const auto& declaration = v.syntax_.declarations[d]; current_module = declaration.module;
            if (type_id(declaration.name) != invalid_id) report(declaration.origin,"Declaration conflicts with a prelude type", "GNR2207");
            if (!valid_identifier(declaration.name)) report(declaration.origin,"Declaration name conflicts with a reserved native identifier");
            const auto qualified = namespace_name(v.syntax_.modules[current_module].name) + "::" + declaration.name;
            const auto value_type = declaration.kind == DeclarationKind::function ? type_id("Callable") : intern(qualified, qualified);
            const auto id = symbol({ResolvedSymbolKind::declaration, declaration.name, qualified, value_type});
            v.declarations_[d].symbol = id; declaration_ids[id] = d;
            if (!declarations.emplace(std::to_string(current_module) + ":" + declaration.name, id).second) report(declaration.origin, "Duplicate declaration '" + declaration.name + "'", "GNR2207");
            for (const auto& import : v.syntax_.modules[current_module].imports) if (import.alias == declaration.name) report(import.origin, "Import alias conflicts with a declaration", "GNR2103");
        }
        for (std::size_t d = 0; d < v.syntax_.declarations.size(); ++d) {
            auto& declaration = v.syntax_.declarations[d]; auto& resolved = v.declarations_[d]; current_module = declaration.module; current_owner = resolved.symbol;
            // Metadata-only models still expose conventional typed attributes.
            if (declaration.kind == DeclarationKind::model) {
                std::set<std::string> attributes;
                for (const auto& metadata : declaration.metadata) {
                    const auto& expression = v.syntax_.expressions[metadata.value];
                    if (metadata.name == "fillable" || metadata.name == "hidden" || metadata.name == "visible")
                        for (auto id : expression.operands) if (v.syntax_.expressions[id].literal_type == "string") attributes.insert(v.syntax_.expressions[id].text);
                    if (metadata.name == "casts") for (const auto& key : expression.argument_names) attributes.insert(key);
                }
                std::string primary = "id"; for (const auto& metadata : declaration.metadata) if (metadata.name == "primaryKey") primary = v.syntax_.expressions[metadata.value].text;
                attributes.insert(primary);
                for (const auto& name : attributes) if (std::none_of(declaration.fields.begin(), declaration.fields.end(), [&](const auto& field) { return field.name == name; })) {
                    std::string type_name = name == primary ? "int" : "string";
                    for (const auto& metadata : declaration.metadata) if (metadata.name == "casts") {
                        const auto& expression = v.syntax_.expressions[metadata.value];
                        for (std::size_t i = 0; i < expression.argument_names.size(); ++i) if (expression.argument_names[i] == name) {
                            const auto& cast = v.syntax_.expressions[expression.operands[i]];
                            if (cast.text == "bool" || cast.text == "int" || cast.text == "double") type_name = cast.text;
                            else if (cast.text == "integer") type_name = "int"; else if (cast.text == "decimal") type_name = "Decimal";
                        }
                    }
                    declaration.fields.push_back({declaration.origin, name, {type_name, {}, false, declaration.origin}});
                }
            }
            for (const auto& field : declaration.fields) {
                if (resolve(field.type) == type_id("void")) report(field.origin,"Fields cannot have type void");
                if (!valid_identifier(field.name)) report(field.origin,"Field name is not a valid native identifier");
                const auto id = symbol({field.injection ? ResolvedSymbolKind::injection : ResolvedSymbolKind::field, field.name, field.name, resolve(field.type), current_owner,
                    declaration.kind == DeclarationKind::event || declaration.kind == DeclarationKind::mail || declaration.kind == DeclarationKind::notification || declaration.kind == DeclarationKind::job, false, field.visibility});
                if (!members.emplace(std::to_string(current_owner) + ":" + field.name, id).second) report(field.origin, "Duplicate field '" + field.name + "'"); resolved.fields.push_back(id);
            }
            for (const auto& method : declaration.methods) {
                if (!valid_identifier(method.name)) report(method.origin,"Method name conflicts with a reserved native identifier");
                ResolvedSymbol callable{ResolvedSymbolKind::callable, method.name, method.name, resolve(method.result), current_owner, true, method.asynchronous, method.visibility};
                bool seen_default = false;
                for (const auto& parameter : method.parameters) {
                    if (resolve(parameter.type) == type_id("void")) report(parameter.origin,"Parameters cannot have type void");
                    callable.parameters.push_back(resolve(parameter.type)); callable.parameter_names.push_back(parameter.name); callable.defaults.push_back(parameter.default_value);
                    if (seen_default && parameter.default_value == invalid_id) report(parameter.origin, "Required parameters must precede default parameters");
                    seen_default |= parameter.default_value != invalid_id;
                }
                auto id = symbol(std::move(callable));
                if (!members.emplace(std::to_string(current_owner) + ":" + method.name, id).second) report(method.origin, "Duplicate method '" + method.name + "'");
                resolved.methods.push_back({id});
                if (declaration.kind == DeclarationKind::function) { declarations[std::to_string(current_module) + ":" + declaration.name] = id; v.symbols_[id].cpp_name = v.symbols_[current_owner].cpp_name; }
            }
        }
        current_owner = invalid_id;
        for (const auto& callable : options.native_callables) {
            ResolvedSymbol value{ResolvedSymbolKind::builtin, callable.name, callable.cpp_name, resolve({callable.result}), invalid_id, true, callable.asynchronous};
            for (const auto& parameter : callable.parameters) { value.parameters.push_back(resolve({parameter.type})); value.parameter_names.push_back(parameter.name); value.defaults.push_back(parameter.optional ? invalid_id - 1 : invalid_id); }
            const auto id = symbol(std::move(value));
            if (callable.owner.empty()) declarations.emplace("native:" + callable.name, id);
        }
    }
    TypeId expression(SyntaxId id, std::optional<TypeId> expected = {}) {
        if (id == invalid_id) return type_id("void");
        const auto e = v.syntax_.expressions.at(id); auto& info = v.expressions_[id];
        auto set = [&](TypeId type, SymbolId symbol_id = invalid_id) { info.type = type; info.symbol = symbol_id; return type; };
        if (e.kind == SyntaxExpressionKind::literal) return set(type_id(e.literal_type));
        if (e.kind == SyntaxExpressionKind::name) {
            const auto symbol_id = visible(e.text, e.origin); if (symbol_id == invalid_id) return set(type_id("Value"));
            const auto& value = v.symbols_[symbol_id]; return set(value.kind == ResolvedSymbolKind::callable ? type_id("Callable") : narrowed.contains(symbol_id) ? unoptional(value.type) : value.type, symbol_id);
        }
        if (e.kind == SyntaxExpressionKind::member) {
            if (e.literal_type == "::") {
                const auto& receiver = v.syntax_.expressions[e.operands[0]];
                if (receiver.kind == SyntaxExpressionKind::name) {
                    const auto qualified = visible(receiver.text + "::" + e.text, e.origin, false);
                    if (qualified != invalid_id) { v.expressions_[e.operands[0]] = {type_id("Callable"),builtin(receiver.text,receiver.text,type_id("Callable"))}; return set(v.symbols_[qualified].type, qualified); }
                }
            }
            const auto receiver = expression(e.operands[0]);
            if (type(receiver).optional && e.literal_type != "?.") report(e.origin, "Optional receiver requires safe access or a null guard");
            auto symbol_id = member(unoptional(receiver), e.text, e.origin);
            if (symbol_id == invalid_id) return set(type_id("Value"));
            auto result = v.symbols_[symbol_id].type; if (e.literal_type == "?.") result = optional(result);
            return set(result, symbol_id);
        }
        if (e.kind == SyntaxExpressionKind::call) return call(id, expected);
        if (e.kind == SyntaxExpressionKind::await_) {
            if (!async_context) report(e.origin, "await requires an async callable", "GNR2210");
            awaited_calls.insert(e.operands[0]);
            const auto value = expression(e.operands[0]); const auto callee = v.expressions_[e.operands[0]].symbol;
            if (v.syntax_.expressions[e.operands[0]].kind != SyntaxExpressionKind::call || callee == invalid_id || !v.symbols_[callee].asynchronous) report(e.origin, "await operand is not an async call", "GNR2211");
            return set(value, callee);
        }
        if (e.kind == SyntaxExpressionKind::list) {
            TypeId element = expected && !type(*expected).arguments.empty() ? type(*expected).arguments[0] : invalid_id;
            for (auto argument : e.operands) { const auto item = expression(argument, element == invalid_id ? std::nullopt : std::optional{element}); if (element == invalid_id) element = item; else if (!assignable(element, item)) element = type_id("Value"); }
            return set(sequence("List", element == invalid_id ? type_id("Value") : element));
        }
        if (e.kind == SyntaxExpressionKind::object) {
            std::unordered_set<std::string> keys;
            for (std::size_t i = 0; i < e.operands.size(); ++i) { if (!keys.insert(e.argument_names[i]).second) report(e.origin, "Duplicate object key '" + e.argument_names[i] + "'"); expression(e.operands[i]); }
            return set(type_id("Json"));
        }
        if (e.kind == SyntaxExpressionKind::lambda) return lambda(id, expected);
        if (e.kind == SyntaxExpressionKind::subscript) {
            const auto container = expression(e.operands[0]), key = expression(e.operands[1]);
            if ((type(container).name == "List" || type(container).name == "Collection") && key == type_id("int")) return set(type(container).arguments[0]);
            if (type(container).name == "Map" && key == type_id("string")) return set(type(container).arguments[1]);
            if ((type(container).name == "Json" || type(container).name == "Data") && key == type_id("string")) return set(type_id("Json"));
            report(e.origin, "Invalid subscript type"); return set(type_id("Value"));
        }
        if (e.kind == SyntaxExpressionKind::unary) {
            const auto operand = expression(e.operands[0]);
            if (e.text == "!") { if (operand != type_id("bool")) report(e.origin, "Logical negation requires bool"); return set(type_id("bool")); }
            if (!numeric(operand)) report(e.origin, "Numeric unary operator requires a number");
            if (e.text.starts_with("post")) writable(e.operands[0]); return set(operand);
        }
        if (e.kind == SyntaxExpressionKind::conditional) {
            if (expression(e.operands[0]) != type_id("bool")) report(e.origin, "Conditional expression requires bool");
            const auto yes = expression(e.operands[1], expected), no = expression(e.operands[2], expected);
            if (!assignable(yes, no)) report(e.origin, "Conditional branches have incompatible types"); return set(yes);
        }
        const auto lhs = expression(e.operands[0]);
        if (e.text == "??") {
            std::function<bool(SyntaxId)> contains_await = [&](SyntaxId id) { const auto& node = v.syntax_.expressions[id]; if (node.kind == SyntaxExpressionKind::await_) return true; if (node.kind == SyntaxExpressionKind::lambda) return false; for (auto child : node.operands) if (contains_await(child)) return true; return false; };
            if (contains_await(e.operands[0]) || contains_await(e.operands[1])) report(e.origin,"await in null coalescing requires a separate binding"); if (!type(lhs).optional) report(e.origin, "Null coalescing requires an optional value"); const auto base = unoptional(lhs); if (!assignable(base, expression(e.operands[1], base))) report(e.origin, "Incompatible coalescing value"); return set(base); }
        const auto rhs = expression(e.operands[1], lhs);
        if (e.text == "=" || e.text == "+=" || e.text == "-=" || e.text == "*=" || e.text == "/=") { writable(e.operands[0]); if (e.text != "=" && !(numeric(lhs) && numeric(rhs)) && !(e.text == "+=" && lhs == type_id("string") && rhs == lhs)) report(e.origin,"Compound assignment requires compatible numeric or string operands"); if (!assignable(lhs, rhs)) report(e.origin, "Assignment type mismatch"); return set(lhs); }
        if (e.text == "&&" || e.text == "||") { if (lhs != type_id("bool") || rhs != type_id("bool")) report(e.origin, "Logical operands must be bool"); return set(type_id("bool")); }
        if (e.text == "==" || e.text == "!=") { if (!(numeric(unoptional(lhs)) && numeric(unoptional(rhs))) && unoptional(lhs) != type_id("string") && unoptional(lhs) != type_id("bool") && type(lhs).name != "null" && type(rhs).name != "null") report(e.origin,"Equality is supported for scalar and optional scalar values"); if (!assignable(lhs, rhs) && !assignable(rhs, lhs)) report(e.origin, "Incompatible equality operands"); return set(type_id("bool")); }
        if (e.text == "<" || e.text == ">" || e.text == "<=" || e.text == ">=") { if (!(numeric(lhs) && numeric(rhs)) && !(lhs == type_id("string") && rhs == lhs)) report(e.origin, "Incompatible comparison operands"); return set(type_id("bool")); }
        if (e.text == "+" && lhs == type_id("string") && rhs == lhs) return set(lhs);
        if (!numeric(lhs) || !numeric(rhs)) report(e.origin, "Arithmetic operands must be numbers");
        if (e.text == "%" && (lhs != type_id("int") || rhs != lhs)) report(e.origin, "Remainder operands must be integers");
        return set(lhs == type_id("double") || rhs == type_id("double") || lhs == type_id("decimal") || rhs == type_id("decimal") ? type_id("double") : lhs);
    }
    void writable(SyntaxId id) {
        const auto& expression = v.syntax_.expressions[id]; const auto symbol = v.expressions_[id].symbol;
        if (expression.kind != SyntaxExpressionKind::name && expression.kind != SyntaxExpressionKind::member && expression.kind != SyntaxExpressionKind::subscript) report(expression.origin, "Assignment requires a writable target");
        if (symbol != invalid_id && v.symbols_[symbol].immutable) report(expression.origin, "Cannot modify an immutable binding", "GNR2208");
        if (expression.kind == SyntaxExpressionKind::subscript) writable(expression.operands[0]);
    }
    TypeId lambda(SyntaxId id, std::optional<TypeId> expected) {
        const auto e = v.syntax_.expressions[id]; auto& info = v.expressions_[id];
        auto previous_async = async_context; auto previous_return = return_type; auto previous_returns = std::move(returned); const auto previous_loops = loops;
        const auto first_lambda_symbol = v.symbols_.size();
        async_context = false; return_type = invalid_id; returned.clear(); loops = 0; scopes.emplace_back();
        std::unordered_set<SymbolId> captures; capture_sets.push_back(&captures);
        const auto contextual = expected && type(*expected).name == "Function" ? type(*expected).arguments : std::vector<TypeId>{};
        for (std::size_t i = 0; i < e.parameters.size(); ++i) {
            const auto& parameter = e.parameters[i];
            auto type_id = parameter.type.name.empty() ? (i + 1 < contextual.size() ? contextual[i] : invalid_id) : resolve(parameter.type);
            if (type_id == invalid_id) { report(parameter.origin, "Lambda parameter needs a type or a typed callback context"); type_id = this->type_id("Value"); }
            const auto symbol_id = symbol({ResolvedSymbolKind::parameter, parameter.name, parameter.name, type_id, invalid_id, true}); info.parameters.push_back(symbol_id); bind(parameter.name, symbol_id, parameter.origin);
        }
        statements(e.body);
        const auto inferred = returned.empty() ? type_id("void") : returned.front();
        for (auto type : returned) if (!assignable(inferred, type)) report(e.origin, "Lambda returns incompatible types");
        if (inferred != type_id("void") && !all_returns(e.body)) report(e.origin, "Lambda may finish without returning a value");
        std::erase_if(captures,[&](auto id){ return id >= first_lambda_symbol; });
        info.captures.assign(captures.begin(), captures.end()); std::sort(info.captures.begin(), info.captures.end()); capture_sets.pop_back(); if (!capture_sets.empty()) capture_sets.back()->insert(captures.begin(),captures.end()); scopes.pop_back();
        std::vector<TypeId> signature; for (auto id : info.parameters) signature.push_back(v.symbols_[id].type); signature.push_back(inferred);
        if (!contextual.empty()) {
            if (signature.size() != contextual.size()) report(e.origin,"Callback parameter count does not match its context");
            else { for (std::size_t i = 0; i + 1 < signature.size(); ++i) if (signature[i] != contextual[i]) report(e.origin,"Callback parameter type does not match its context"); if (type(contextual.back()).name != "Value" && !assignable(contextual.back(),inferred)) report(e.origin,"Callback return type does not match its context"); }
        }
        info.type = intern("Function", "auto", signature);
        async_context = previous_async; return_type = previous_return; returned = std::move(previous_returns); loops = previous_loops;
        return info.type;
    }
    TypeId call(SyntaxId id, std::optional<TypeId> expected) {
        const auto e = v.syntax_.expressions[id]; auto& info = v.expressions_[id]; const auto callee = v.syntax_.expressions[e.operands[0]];
        SymbolId callable_id = invalid_id; TypeId receiver = invalid_id; SyntaxId receiver_expression = invalid_id; std::string name;
        if (callee.kind == SyntaxExpressionKind::name) { name = callee.text; callable_id = visible(name, callee.origin, false); }
        else if (callee.kind == SyntaxExpressionKind::member) {
            name = callee.text; receiver_expression = callee.operands[0];
            const auto& prefix = v.syntax_.expressions[receiver_expression];
            if (callee.literal_type == "::" && prefix.kind == SyntaxExpressionKind::name) { callable_id = visible(prefix.text + "::" + name, callee.origin, false); if (callable_id != invalid_id) v.expressions_[receiver_expression] = {type_id("Callable"), builtin(prefix.text,prefix.text,type_id("Callable"))}; }
            if (callable_id == invalid_id) { receiver = expression(receiver_expression); if (type(receiver).optional || callee.literal_type == "?.") report(callee.origin,"Optional method calls require an explicit null guard"); callable_id = member(unoptional(receiver), name, callee.origin, false); }
        } else { report(callee.origin, "Expression is not callable"); }
        const auto count = e.operands.size() - 1;
        std::vector<TypeId> argument_types;
        auto arity = [&](std::size_t low, std::size_t high) { if (count < low || count > high) report(e.origin, "Incorrect argument count for '" + name + "'", "GNR2209"); };
        auto finish_builtin = [&](TypeId result, std::string cpp, std::vector<std::optional<TypeId>> contexts = {}) {
            for (std::size_t i = 0; i < count; ++i) { argument_types.push_back(expression(e.operands[i + 1], i < contexts.size() ? contexts[i] : std::nullopt)); if (i < contexts.size() && contexts[i] && type(*contexts[i]).name != "Function" && !assignable(*contexts[i],argument_types.back())) report(e.origin,"Builtin argument type mismatch"); }
            callable_id = builtin(name, std::move(cpp), result, argument_types);
            info.type = result; info.symbol = callable_id;
            v.expressions_[e.operands[0]] = {type_id("Callable"), callable_id};
            for (std::size_t i = 0; i < count; ++i) { info.argument_order.push_back(i); info.argument_conversions.push_back(i < contexts.size() && contexts[i] && type(*contexts[i]).name != "Function" ? *contexts[i] : argument_types[i]); if (!e.argument_names[i].empty()) report(e.origin, "Named arguments require a declared callable signature"); }
            return result;
        };
        if (callable_id != invalid_id && v.symbols_[callable_id].kind == ResolvedSymbolKind::declaration) {
            const auto owner = callable_id; const auto& declaration = v.syntax_.declarations[declaration_ids.at(owner)];
            if (std::any_of(declaration.fields.begin(),declaration.fields.end(),[](const auto& field){return field.injection;})) report(e.origin,"Declarations with injected services use the native make(container, ...) factory");
            ResolvedSymbol constructor{ResolvedSymbolKind::callable, declaration.name, v.symbols_[owner].cpp_name, v.symbols_[owner].type, owner};
            for (auto field : v.declarations_[declaration_ids.at(owner)].fields) if (v.symbols_[field].kind == ResolvedSymbolKind::field) { constructor.parameters.push_back(v.symbols_[field].type); constructor.parameter_names.push_back(v.symbols_[field].name); auto original = std::find_if(declaration.fields.begin(),declaration.fields.end(),[&](const auto& f){return f.name == v.symbols_[field].name;}); constructor.defaults.push_back(original == declaration.fields.end() || original->initializer == invalid_id || v.syntax_.expressions[original->initializer].kind != SyntaxExpressionKind::literal ? invalid_id : original->initializer); }
            if (declaration.kind == DeclarationKind::model) { if (count) report(e.origin, "Models use ORM creation rather than data constructors"); constructor.parameters.clear(); constructor.parameter_names.clear(); constructor.defaults.clear(); }
            callable_id = symbol(std::move(constructor));
        }
        if (callable_id == invalid_id && receiver == invalid_id) {
            if (name == "text" || name == "html" || name == "json" || name == "view" || name == "redirect" || name == "response") { arity(1, name == "view" ? 3 : 2); return finish_builtin(type_id("Response"), "gungnir::language::runtime::" + name,
                name == "view" ? std::vector<std::optional<TypeId>>{type_id("string"),type_id("Json"),type_id("int")} : std::vector<std::optional<TypeId>>{name == "json" ? type_id("Json") : type_id("string"),type_id("int")}); }
            if (name == "exactDecimal") { arity(1,1); return finish_builtin(type_id("Decimal"), "gungnir::model::Decimal", {type_id("string")}); }
            if (name == "authorize") { arity(3,3); return finish_builtin(type_id("void"), "gungnir::language::runtime::authorize", {type_id("Request"),type_id("string"),std::nullopt}); }
            if (name == "allow" || name == "deny") { arity(0, name == "allow" ? 0 : 1); return finish_builtin(type_id("Decision"), "gungnir::auth::Decision::" + name, {type_id("string")}); }
        }
        if (callable_id == invalid_id && receiver != invalid_id) {
            const auto t = type(unoptional(receiver));
            for (const auto& binding : options.native_callables) if (binding.owner == t.name && binding.name == name) {
                ResolvedSymbol callable{ResolvedSymbolKind::builtin, name, binding.cpp_name, resolve({binding.result}), owner_of(receiver), true, binding.asynchronous};
                for (const auto& parameter : binding.parameters) { callable.parameters.push_back(resolve({parameter.type})); callable.parameter_names.push_back(parameter.name); callable.defaults.push_back(parameter.optional ? invalid_id - 1 : invalid_id); }
                callable_id = symbol(std::move(callable)); break;
            }
            if (callable_id == invalid_id && t.name == "Table") {
                if (name == "create" || name == "alter") { arity(2, 2); return finish_builtin(type_id("void"), name, {type_id("string"), intern("Function", "auto", {type_id("Column"), type_id("void")})}); }
                if (name == "drop" || name == "dropIfExists" || name == "rename") { arity(name == "rename" ? 2 : 1, name == "rename" ? 2 : 1); return finish_builtin(type_id("void"), snake(name)); }
            }
            if (callable_id == invalid_id && t.name == "Column") {
                static const std::unordered_set<std::string> columns{"id","string","text","integer","tinyInteger","smallInteger","mediumInteger","bigInteger","boolean","decimal","float","double","json","uuid","date","time","datetime","timestamp","timestampTz","binary","foreignId","softDeletes"};
                static const std::unordered_set<std::string> commands{"timestamps","timestampsTz","dropTimestamps","dropSoftDeletes","drop","rename","dropColumn","dropIndex","dropForeign","dropUnique","renameIndex"};
                if (columns.contains(name)) { arity(name == "id" || name == "softDeletes" ? 0 : 1, name == "decimal" ? 3 : name == "string" ? 2 : 1); return finish_builtin(type_id("ColumnDefinition"), name == "datetime" ? "date_time" : name == "float" ? "floating" : name == "double" ? "double_precision" : snake(name)); }
                if (commands.contains(name)) { arity(name.starts_with("timestamps") || name == "dropTimestamps" ? 0 : 1, name == "rename" || name == "renameIndex" ? 2 : 1); return finish_builtin(type_id("void"), name == "dropColumn" ? "drop" : snake(name)); }
                if (name == "index" || name == "unique" || name == "primary" || name == "foreign") { arity(1, 2); return finish_builtin(type_id(name == "foreign" ? "ForeignKeyDefinition" : "IndexDefinition"), name); }
            }
            if (callable_id == invalid_id && (t.name == "ColumnDefinition" || t.name == "IndexDefinition" || t.name == "ForeignKeyDefinition")) {
                static const std::unordered_set<std::string> modifiers{"nullable","defaultValue","unique","index","primary","unsigned","references","on","onDelete","onUpdate","cascadeOnDelete","cascadeOnUpdate","restrictOnDelete","nullOnDelete"};
                if (modifiers.contains(name)) { arity(0, 1); return finish_builtin(unoptional(receiver), snake(name)); }
            }
            if (callable_id == invalid_id && t.name == "Decimal" && (name == "string" || name == "toDouble")) { arity(0,0); return finish_builtin(type_id(name == "string" ? "string" : "double"), name == "string" ? "string" : "to_double"); }
            if (callable_id == invalid_id && t.name == "Request") {
                if (name == "structuredInput") { arity(0,0); return finish_builtin(type_id("Json"), "structured_input"); }
                if (name == "validate") { arity(1,1); return finish_builtin(type_id("Json"), "gungnir::language::runtime::validate", {type_id("Json")}); }
                static const std::unordered_set<std::string> strings{"header","parameter","query","input","body","path","target"};
                if (strings.contains(name)) { const auto count = name == "body" || name == "path" || name == "target" ? 0 : 1; arity(count,count); return finish_builtin(type_id("string"), name, {type_id("string")}); }
            }
            const auto owner = owner_of(receiver);
            const bool model = owner != invalid_id && v.syntax_.declarations[declaration_ids.at(owner)].kind == DeclarationKind::model;
            if (callable_id == invalid_id && (model || t.name == "Query")) {
                const auto model_type = model ? unoptional(receiver) : t.arguments[0];
                if (name == "all" || name == "get") { arity(0, 0); return finish_builtin(sequence("Collection", model_type), name); }
                if (name == "findOrFail" || name == "firstOrFail" || name == "create") { arity(name == "firstOrFail" ? 0 : 1, name == "firstOrFail" ? 0 : 1); return finish_builtin(model_type, snake(name)); }
                if (name == "find" || name == "first") { arity(name == "first" ? 0 : 1, name == "first" ? 0 : 1); return finish_builtin(optional(model_type), name); }
                if (name == "count") { arity(0, 0); return finish_builtin(type_id("int"), name); }
                if (name == "save" || name == "remove" || name == "update") { arity(name == "update" ? 1 : 0, name == "update" ? 1 : 0); return finish_builtin(type_id("bool"), name); }
                static const std::unordered_set<std::string> query{"query","where","whereIn","orderBy","orderByDesc","with","limit","offset","select","withDeleted","onlyDeleted"};
                if (query.contains(name)) { arity(name == "query" || name == "withDeleted" || name == "onlyDeleted" ? 0 : 1, name == "where" ? 2 : name == "orderBy" ? 2 : 1); return finish_builtin(sequence("Query", model_type), snake(name)); }
            }
            if (callable_id == invalid_id && (t.name == "List" || t.name == "Collection")) {
                const auto element = t.arguments[0];
                if (name == "count" || name == "size") { arity(0, 0); return finish_builtin(type_id("int"), "size"); }
                if (name == "empty" || name == "isEmpty") { arity(0, 0); return finish_builtin(type_id("bool"), "empty"); }
                if (name == "map" || name == "filter" || name == "each") {
                    arity(1, 1); const auto callback = count ? expression(e.operands[1], intern("Function", "auto", {element, type_id("Value")})) : type_id("Callable");
                    if (name == "filter" && (type(callback).arguments.empty() || type(callback).arguments.back() != type_id("bool"))) report(e.origin,"filter callback must return bool");
                    if (name == "map" && !type(callback).arguments.empty() && type(callback).arguments.back() == type_id("void")) report(e.origin,"map callback must return a value; use each for void callbacks");
                    auto result = name == "each" ? type_id("void") : sequence("List", name == "map" && !type(callback).arguments.empty() ? type(callback).arguments.back() : element);
                    callable_id = builtin(name, "gungnir::language::runtime::" + name, result, {callback}); info.type = result; info.symbol = callable_id; info.argument_order = {0}; info.argument_conversions = {callback}; v.expressions_[e.operands[0]] = {type_id("Callable"), callable_id}; return result;
                }
            }
            if (callable_id == invalid_id && t.name == "string" && (name == "size" || name == "length" || name == "empty")) { arity(0,0); return finish_builtin(type_id(name == "empty" ? "bool" : "int"), name == "length" ? "size" : name); }
        }
        if (callable_id == invalid_id || (v.symbols_[callable_id].kind != ResolvedSymbolKind::callable && v.symbols_[callable_id].kind != ResolvedSymbolKind::builtin && v.symbols_[callable_id].type != type_id("Callable") && type(v.symbols_[callable_id].type).name != "Function")) {
            report(e.origin, "Unknown callable '" + name + "'", "GNR2212"); for (std::size_t i = 1; i < e.operands.size(); ++i) expression(e.operands[i]); info.type = type_id("Value"); return info.type;
        }
        if (callable_id != invalid_id && type(v.symbols_[callable_id].type).name == "Function") {
            const auto binding = v.symbols_[callable_id]; const auto parameters = type(binding.type).arguments;
            ResolvedSymbol callable{ResolvedSymbolKind::callable,binding.name,binding.cpp_name,parameters.back(),callable_id};
            callable.parameters.assign(parameters.begin(),parameters.end()-1);
            for (std::size_t i = 0; i < callable.parameters.size(); ++i) { callable.parameter_names.push_back("argument" + std::to_string(i)); callable.defaults.push_back(invalid_id); }
            callable_id = symbol(std::move(callable));
        }
        const auto signature = v.symbols_[callable_id];
        if (callee.kind == SyntaxExpressionKind::member && callee.literal_type == "::" && signature.kind == ResolvedSymbolKind::callable && signature.owner != invalid_id && declaration_ids.contains(signature.owner) && v.syntax_.declarations[declaration_ids.at(signature.owner)].kind != DeclarationKind::function)
            report(callee.origin,"Instance methods require an instance receiver");
        std::vector<std::size_t> order(signature.parameters.size(), invalid_id);
        bool named = false; std::size_t position = 0;
        for (std::size_t i = 0; i < count; ++i) {
            std::size_t target = position++;
            if (!e.argument_names[i].empty()) { named = true; const auto found = std::find(signature.parameter_names.begin(), signature.parameter_names.end(), e.argument_names[i]); target = static_cast<std::size_t>(found - signature.parameter_names.begin()); }
            else if (named) report(e.origin, "Positional arguments must precede named arguments");
            if (target >= order.size()) { report(e.origin, "Unknown or excessive argument", "GNR2209"); expression(e.operands[i+1]); continue; }
            if (order[target] != invalid_id) report(e.origin, "Argument supplied more than once", "GNR2209");
            order[target] = i; const auto actual = expression(e.operands[i + 1], signature.parameters[target]);
            if (!assignable(signature.parameters[target], actual)) report(v.syntax_.expressions[e.operands[i + 1]].origin, "Call argument type mismatch", "GNR2213");
        }
        for (std::size_t i = 0; i < order.size(); ++i) if (order[i] == invalid_id && (i >= signature.defaults.size() || signature.defaults[i] == invalid_id)) report(e.origin, "Missing required argument '" + signature.parameter_names[i] + "'", "GNR2209");
        info.type = signature.type; info.symbol = callable_id; info.argument_order = std::move(order); info.argument_conversions = signature.parameters;
        v.expressions_[e.operands[0]] = {type_id("Callable"), callable_id};
        if (signature.type == type_id("inferred")) report(e.origin, "Callable result has not been resolved");
        (void)expected; return info.type;
    }
    bool all_returns(const std::vector<SyntaxId>& body) const {
        for (auto id : body) { const auto& statement = v.syntax_.statements[id]; if (statement.kind == SyntaxStatementKind::return_ || statement.kind == SyntaxStatementKind::throw_) return true;
            if (statement.kind == SyntaxStatementKind::block && all_returns(statement.body)) return true;
            if (statement.kind == SyntaxStatementKind::if_ && !statement.alternative.empty() && all_returns(statement.body) && all_returns(statement.alternative)) return true;
        } return false;
    }
    void statements(const std::vector<SyntaxId>& body) {
        for (auto id : body) {
            const auto s = v.syntax_.statements[id];
            if (s.kind == SyntaxStatementKind::binding) {
                const auto expected = s.declared_type ? std::optional{resolve(*s.declared_type)} : std::nullopt;
                const auto actual = expression(s.expression, expected); if (actual == type_id("void")) report(s.origin, "A binding cannot contain void");
                if (expected && !assignable(*expected, actual)) report(s.origin, "Binding type mismatch");
                auto binding = symbol({ResolvedSymbolKind::local,s.name,s.name,expected.value_or(actual),invalid_id,s.immutable}); v.bindings_[id] = binding; bind(s.name,binding,s.origin);
            } else if (s.kind == SyntaxStatementKind::return_) {
                const auto actual = expression(s.expression, return_type == invalid_id ? std::nullopt : std::optional{return_type}); returned.push_back(actual);
                if (return_type != invalid_id && return_type != type_id("inferred") && !assignable(return_type,actual)) report(s.origin, "Return type mismatch", "GNR2214");
            } else if (s.kind == SyntaxStatementKind::break_ || s.kind == SyntaxStatementKind::continue_) { if (!loops) report(s.origin, "Loop control outside a loop"); }
            else if (s.kind == SyntaxStatementKind::if_ || s.kind == SyntaxStatementKind::while_) {
                if (expression(s.expression) != type_id("bool")) report(s.origin,"Condition must be bool");
                const auto previous = narrowed; SymbolId guarded = invalid_id; bool nonnull = false;
                const auto& condition = v.syntax_.expressions[s.expression];
                if (condition.kind == SyntaxExpressionKind::binary && (condition.text == "!=" || condition.text == "==")) {
                    const auto a = condition.operands[0], b = condition.operands[1];
                    const auto candidate = v.syntax_.expressions[a].literal_type == "null" ? b : v.syntax_.expressions[b].literal_type == "null" ? a : invalid_id;
                    if (candidate != invalid_id && v.syntax_.expressions[candidate].kind == SyntaxExpressionKind::name) { guarded = v.expressions_[candidate].symbol; nonnull = condition.text == "!="; }
                }
                if (guarded != invalid_id && nonnull) narrowed.insert(guarded);
                scopes.emplace_back(); if (s.kind == SyntaxStatementKind::while_) ++loops; statements(s.body); if (s.kind == SyntaxStatementKind::while_) --loops; scopes.pop_back();
                narrowed = previous; if (guarded != invalid_id && !nonnull) narrowed.insert(guarded);
                scopes.emplace_back(); statements(s.alternative); scopes.pop_back(); narrowed = previous;
            } else if (s.kind == SyntaxStatementKind::for_) {
                scopes.emplace_back(); statements(s.parts); if (s.expression != invalid_id && expression(s.expression) != type_id("bool")) report(s.origin,"Loop condition must be bool"); ++loops; scopes.emplace_back(); statements(s.body); scopes.pop_back(); statements(s.alternative); --loops; scopes.pop_back();
            } else if (s.kind == SyntaxStatementKind::for_in) {
                const auto collection = expression(s.expression); if (type(collection).arguments.empty()) { report(s.origin,"for-in requires a collection"); continue; }
                scopes.emplace_back(); auto binding = symbol({ResolvedSymbolKind::local,s.name,s.name,type(collection).arguments[0],invalid_id,true}); v.bindings_[id] = binding; bind(s.name,binding,s.origin); ++loops; statements(s.body); --loops; scopes.pop_back();
            } else if (s.kind == SyntaxStatementKind::block) { scopes.emplace_back(); statements(s.body); scopes.pop_back(); }
            else if (s.expression != invalid_id) expression(s.expression);
        }
    }
    void contracts(std::size_t d) {
        auto& declaration = v.syntax_.declarations[d]; auto& resolved = v.declarations_[d];
        if (declaration.kind == DeclarationKind::event && (!declaration.methods.empty() || !declaration.metadata.empty() || std::any_of(declaration.fields.begin(),declaration.fields.end(),[](const auto& f){return f.injection;}))) report(declaration.origin,"Events contain data fields only");
        if (declaration.kind == DeclarationKind::migration) for (const auto& name : {"up","down"}) {
            auto found = std::find_if(declaration.methods.begin(),declaration.methods.end(),[&](const auto& method){return method.name == name;});
            if (found == declaration.methods.end() || !found->parameters.empty() || found->result.name != "void" || found->asynchronous) report(declaration.origin,"Migration requires synchronous void up() and down() methods");
        }
        if (declaration.kind == DeclarationKind::listener || declaration.kind == DeclarationKind::job) {
            auto found = std::find_if(declaration.methods.begin(),declaration.methods.end(),[](const auto& method){return method.name == "handle";});
            if (found == declaration.methods.end()) report(declaration.origin,"This declaration requires handle()");
            else if (declaration.kind == DeclarationKind::listener) {
                if (found->parameters.size() != 1) report(found->origin,"Listener handle requires one event parameter");
                else { const auto event = owner_of(resolve(found->parameters[0].type)); if (event == invalid_id || v.syntax_.declarations[declaration_ids[event]].kind != DeclarationKind::event) report(found->origin,"Listener parameter must be an event"); }
            } else if (!found->parameters.empty()) report(found->origin,"Job handle() does not accept parameters");
        }
        if (declaration.kind == DeclarationKind::mail) for (const auto& name : {"subject"}) if (std::none_of(declaration.methods.begin(),declaration.methods.end(),[&](const auto& method){return method.name == name;})) report(declaration.origin,"Mail requires subject()");
        if (declaration.kind == DeclarationKind::notification && std::none_of(declaration.methods.begin(),declaration.methods.end(),[](const auto& method){return method.name == "via";})) report(declaration.origin,"Notification requires via()");
        if (declaration.kind == DeclarationKind::listener || declaration.kind == DeclarationKind::job) for (const auto& method : resolved.methods) if (v.symbols_[method.symbol].name == "handle" && v.symbols_[method.symbol].type != type_id("void")) report(declaration.origin,"handle() must return void");
        if (declaration.kind == DeclarationKind::notification) for (std::size_t m = 0; m < declaration.methods.size(); ++m) if (declaration.methods[m].name == "via" && (declaration.methods[m].asynchronous || declaration.methods[m].parameters.size() != 1)) report(declaration.origin,"via() requires one recipient and must be synchronous");
        if (declaration.kind == DeclarationKind::mail) for (const auto& method : declaration.methods) if ((method.name == "subject" || method.name == "text" || method.name == "html" || method.name == "content") && (method.asynchronous || !method.parameters.empty())) report(method.origin,"Mail composition methods must be synchronous and parameterless");
        for (std::size_t m = 0; m < declaration.methods.size(); ++m) {
            const auto& method = declaration.methods[m]; const auto& fn = v.symbols_[resolved.methods[m].symbol];
            if ((declaration.kind == DeclarationKind::listener || declaration.kind == DeclarationKind::job) && method.name == "handle" && method.visibility != Visibility::public_)
                report(method.origin,"handle() must be public");
            if (declaration.kind == DeclarationKind::mail && (method.name == "subject" || method.name == "text" || method.name == "html") && fn.type != type_id("string"))
                report(method.origin,"Mail composition must return string");
            if (declaration.kind == DeclarationKind::mail && method.name == "content" && fn.type != type_id("Response")) report(method.origin,"Mail content must return Response");
            if (declaration.kind == DeclarationKind::notification && method.name == "via" && fn.type != sequence("List",type_id("string")))
                report(method.origin,"via() must return List<string>");
            if (declaration.kind == DeclarationKind::notification && (method.name == "toMail" || method.name == "toDatabase")) {
                const auto via = std::find_if(declaration.methods.begin(),declaration.methods.end(),[](const auto& method){return method.name == "via";});
                if (via != declaration.methods.end()) { const auto& via_fn = v.symbols_[resolved.methods[via-declaration.methods.begin()].symbol]; if (fn.parameters != via_fn.parameters || method.asynchronous || method.visibility != Visibility::public_) report(method.origin,"Notification methods require the same recipient contract as via()"); }
            }
        }
        std::unordered_set<std::string> metadata_names;
        static const std::unordered_set<std::string> metadata_allowed{"table","connection","fillable","hidden","visible","casts","timestamps","softDeletes","primaryKey","incrementing"};
        for (const auto& metadata : declaration.metadata) {
            if (declaration.kind != DeclarationKind::model || !metadata_allowed.contains(metadata.name)) { report(metadata.origin,"Unknown framework metadata '" + metadata.name + "'"); continue; }
            if (!metadata_names.insert(metadata.name).second) report(metadata.origin,"Duplicate metadata '" + metadata.name + "'");
            const auto& expr = v.syntax_.expressions[metadata.value];
            if (metadata.name == "fillable" || metadata.name == "hidden" || metadata.name == "visible") {
                if (expr.kind != SyntaxExpressionKind::list) report(metadata.origin,"Attribute metadata requires a constant string list");
                for (auto id : expr.operands) if (v.syntax_.expressions[id].literal_type != "string") report(metadata.origin,"Attribute names must be string constants");
            } else if (metadata.name == "casts") {
                if (expr.kind != SyntaxExpressionKind::object) report(metadata.origin,"casts requires a constant object");
                static const std::unordered_set<std::string> supported{"bool","int","integer","string","double","decimal","json","date","datetime"};
                for (auto id : expr.operands) if (v.syntax_.expressions[id].literal_type != "string" || !supported.contains(v.syntax_.expressions[id].text)) report(metadata.origin,"Unknown model cast");
            } else if (metadata.name == "timestamps" || metadata.name == "softDeletes" || metadata.name == "incrementing") { if (expr.literal_type != "bool") report(metadata.origin,"Metadata requires a boolean constant"); }
            else if (expr.literal_type != "string") report(metadata.origin,"Metadata requires a string constant");
            expression(metadata.value);
        }
        (void)resolved;
    }
    void declaration_dependencies() {
        std::vector<unsigned> state(v.syntax_.declarations.size());
        std::function<void(std::size_t)> visit = [&](std::size_t d) {
            if (state[d] == 2) return;
            if (state[d] == 1) { report(v.syntax_.declarations[d].origin,"Circular declaration value dependency", "GNR2217"); return; }
            state[d] = 1;
            const auto& declaration = v.syntax_.declarations[d];
            for (auto field : v.declarations_[d].fields) { const auto owner = owner_of(v.symbols_[field].type); if (owner != invalid_id) visit(declaration_ids.at(owner)); }
            if (declaration.kind == DeclarationKind::listener || declaration.kind == DeclarationKind::notification || declaration.kind == DeclarationKind::policy) for (const auto& method : v.declarations_[d].methods) for (auto parameter : v.symbols_[method.symbol].parameters) { const auto owner = owner_of(parameter); if (owner != invalid_id && owner != v.declarations_[d].symbol) visit(declaration_ids.at(owner)); }
            state[d] = 2; v.declaration_order_.push_back(d);
        };
        for (auto module : v.module_order_) for (auto d : v.syntax_.modules[module].declarations) visit(d);
    }
    void bodies() {
        // Resolve contextual notification result types before other callables.
        std::vector<std::pair<std::size_t,std::size_t>> work;
        for (std::size_t d = 0; d < v.syntax_.declarations.size(); ++d) for (std::size_t m = 0; m < v.syntax_.declarations[d].methods.size(); ++m) work.emplace_back(d,m);
        std::stable_sort(work.begin(),work.end(),[&](auto a,auto b){return (v.syntax_.declarations[a.first].methods[a.second].result.name == "inferred") > (v.syntax_.declarations[b.first].methods[b.second].result.name == "inferred");});
        for (auto [d,m] : work) {
            const auto method = v.syntax_.declarations[d].methods[m]; auto& resolved = v.declarations_[d].methods[m]; current_module = v.syntax_.declarations[d].module; current_owner = v.declarations_[d].symbol;
            scopes.clear(); narrowed.clear(); scopes.emplace_back(); async_context = method.asynchronous; loops = 0; return_type = v.symbols_[resolved.symbol].type; returned.clear();
            for (std::size_t p = 0; p < method.parameters.size(); ++p) {
                const auto& parameter = method.parameters[p]; const auto parameter_type = v.symbols_[resolved.symbol].parameters[p];
                if (parameter.default_value != invalid_id) { const auto& expression_node = v.syntax_.expressions[parameter.default_value]; if (expression_node.kind != SyntaxExpressionKind::literal) report(parameter.origin,"Default arguments currently require literal constants"); if (!assignable(parameter_type,expression(parameter.default_value,parameter_type))) report(parameter.origin,"Default argument type mismatch"); }
                const auto id = symbol({ResolvedSymbolKind::parameter,parameter.name,parameter.name,parameter_type,resolved.symbol,true}); resolved.parameters.push_back(id); bind(parameter.name,id,parameter.origin);
            }
            statements(method.body); resolved.all_paths_return = all_returns(method.body);
            if (return_type == type_id("inferred")) {
                const auto inferred = returned.empty() ? type_id("void") : returned.front(); for (auto type : returned) if (!assignable(inferred,type)) report(method.origin,"Inferred returns have incompatible types"); v.symbols_[resolved.symbol].type = inferred; return_type = inferred;
            }
            if (return_type != type_id("void") && !resolved.all_paths_return) report(method.origin,"Callable may finish without returning a value", "GNR2215");
        }
        for (std::size_t d = 0; d < v.syntax_.declarations.size(); ++d) {
            current_module = v.syntax_.declarations[d].module; current_owner = v.declarations_[d].symbol; scopes.clear(); scopes.emplace_back(); async_context = false; loops = 0; contracts(d);
            for (std::size_t f = 0; f < v.syntax_.declarations[d].fields.size(); ++f) { const auto& field = v.syntax_.declarations[d].fields[f]; if (field.initializer != invalid_id && !assignable(v.symbols_[v.declarations_[d].fields[f]].type, expression(field.initializer))) report(field.origin,"Field initializer type mismatch"); }
        }
        for (std::size_t i = 0; i < v.expressions_.size(); ++i) if (v.syntax_.expressions[i].kind == SyntaxExpressionKind::call && v.expressions_[i].symbol != invalid_id && v.symbols_[v.expressions_[i].symbol].asynchronous && !awaited_calls.contains(i)) report(v.syntax_.expressions[i].origin,"Async calls require await", "GNR2216");
        // Every reachable expression must have a resolved type. Unreachable
        // syntax is still checked, rather than being smuggled into codegen.
        for (std::size_t i = 0; i < v.expressions_.size(); ++i) if (v.expressions_[i].type == invalid_id) report(v.syntax_.expressions[i].origin,"Expression was not resolved", "GNR2299");
    }
    ValidationResult run() {
        index(); if (diagnostics.empty()) declaration_dependencies(); if (diagnostics.empty()) bodies();
        if (!diagnostics.empty()) return {std::nullopt,std::move(diagnostics)};
        return {std::move(v),{}};
    }
};
ValidationResult ProgramValidator::validate(SyntaxProject project, const CompilerOptions& options) const {
    return ValidationEngine{std::move(project),options}.run();
}
} // namespace gungnir::language
