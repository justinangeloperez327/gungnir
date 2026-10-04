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
        Diagnostic diagnostic{
            DiagnosticLevel::error,
            {origin.file, origin.line, origin.column},
            std::move(message),
            std::move(code),
            {}
        };
        if (!origin.file.empty()) {
            diagnostic.span.begin_offset = origin.begin;
            diagnostic.span.end_offset =
                origin.end > origin.begin ? origin.end : origin.begin + 1;
            diagnostic.span.valid = true;
        }
        diagnostics.push_back(std::move(diagnostic));
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
        return intern(name, (name == "Query" ? "gungnir::orm::Query<" : name == "Collection" ? "gungnir::orm::Collection<" : name == "Page" ? "gungnir::orm::Page<" : "std::vector<") + type(element).cpp_name + ">", {element});
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
        if (name == "List" || name == "Collection" || name == "Query" || name == "Page") {
            if (syntax.arguments.size() != 1) { report(syntax.origin, name + " requires one type argument"); return type_id("Value"); }
            const auto element = resolve(syntax.arguments.front());
            if (name == "Query" || name == "Page") {
                const auto owner = owner_of(element);
                if (type(element).optional || owner == invalid_id || v.syntax_.declarations[declaration_ids.at(owner)].kind != DeclarationKind::model)
                    report(syntax.origin, name + " requires a non-optional model type", "GNR2311");
            }
            value = sequence(name, element);
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
        if (a.name == "double" && b.name == "int") return true;
        if (a.name == "decimal" && (b.name == "int" || b.name == "uint64")) return true;
        if (a.name == "Decision" && b.name == "bool") return true;
        if ((a.name == "Json" || a.name == "Data") && b.name != "void" && b.name != "Callable") return true;
        if (a.name == "List" && b.name == "List" && a.arguments.size() == 1 && b.arguments.size() == 1) return a.arguments[0] == b.arguments[0];
        return false;
    }
    std::optional<TypeId> common_type(TypeId left, TypeId right) {
        if (left == right) return left;
        const auto a = type(left), b = type(right);

        if (a.name == "null") {
            if (b.name == "null") return left;
            if (b.name == "void" || b.name == "Callable") return std::nullopt;
            return b.optional ? right : optional(right);
        }
        if (b.name == "null") {
            if (a.name == "void" || a.name == "Callable") return std::nullopt;
            return a.optional ? left : optional(left);
        }

        if (a.optional || b.optional) {
            const auto left_base = a.optional ? unoptional(left) : left;
            const auto right_base = b.optional ? unoptional(right) : right;
            const auto base = common_type(left_base, right_base);
            return base ? std::optional<TypeId>{optional(*base)} : std::nullopt;
        }

        if (numeric(left) && numeric(right)) {
            const bool int_double =
                (a.name == "int" && b.name == "double") ||
                (a.name == "double" && b.name == "int");
            if (int_double) return type_id("double");

            const bool integer_decimal =
                ((a.name == "int" || a.name == "uint64") && b.name == "decimal") ||
                ((b.name == "int" || b.name == "uint64") && a.name == "decimal");
            if (integer_decimal) return type_id("decimal");

            // Signed/unsigned, exact/approximate and uint64/double mixing have
            // no implicit common type in the current language contract.
            return std::nullopt;
        }

        if (assignable(left, right)) return left;
        if (assignable(right, left)) return right;
        return std::nullopt;
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
        if (type(receiver).name == "Page") {
            const auto& page = type(receiver);
            if (name == "data") return symbol({ResolvedSymbolKind::field, name, name, sequence("Collection", page.arguments[0])});
            static const std::unordered_map<std::string, std::string> fields{
                {"currentPage", "current_page"}, {"perPage", "per_page"},
                {"total", "total"}, {"lastPage", "last_page"}
            };
            if (const auto found = fields.find(name); found != fields.end())
                return symbol({ResolvedSymbolKind::field, name, found->second, type_id("int")});
        }
        if (diagnose) report(origin, "Unknown member '" + name + "' on " + type(receiver).name, "GNR2206"); return invalid_id;
    }
    SymbolId builtin(std::string name, std::string cpp, TypeId result, std::vector<TypeId> parameters = {}, bool async = false) {
        ResolvedSymbol value{ResolvedSymbolKind::builtin, std::move(name), std::move(cpp), result}; value.parameters = std::move(parameters); value.asynchronous = async; return symbol(std::move(value));
    }
    static std::string relationship_type(const std::string& kind) {
        auto result = kind;
        result.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(result.front())));
        return result;
    }
    std::string primary_key(std::size_t declaration) const {
        for (const auto& metadata : v.syntax_.declarations[declaration].metadata)
            if (metadata.name == "primaryKey") {
                const auto& value = v.syntax_.expressions[metadata.value];
                if (value.literal_type == "string") return value.text;
            }
        return "id";
    }
    static std::string foreign_key(const DeclarationSyntax& declaration, const std::string& key) {
        auto result = snake(declaration.name);
        if (result.starts_with('_')) result.erase(0, 1);
        return result + "_" + key;
    }
    TypeId relationship_key_type(std::size_t declaration, const std::string& key, const Origin& origin) {
        const auto& model = v.syntax_.declarations[declaration];
        const auto field = std::find_if(model.fields.begin(), model.fields.end(),
            [&](const auto& value) { return value.name == key; });
        if (field == model.fields.end()) {
            report(origin, "Unknown relationship key '" + key + "' on " + model.name, "GNR2310");
            return type_id("int");
        }
        const auto previous_module = current_module;
        current_module = model.module;
        const auto result = unoptional(resolve(field->type));
        current_module = previous_module;
        if (result != type_id("int") && result != type_id("uint64") && result != type_id("string"))
            report(origin, "Relationship keys require integer or string attributes", "GNR2310");
        return result;
    }
    void relationship_foreign_key(std::size_t declaration, const std::string& key,
                                  TypeId value_type, const Origin& origin) {
        auto& model = v.syntax_.declarations[declaration];
        auto field = std::find_if(model.fields.begin(), model.fields.end(),
            [&](const auto& value) { return value.name == key; });
        if (field == model.fields.end()) {
            model.fields.push_back({origin, key, {type(value_type).name, {}, false, origin}});
        } else if (field->inferred_type) {
            field->type.name = type(value_type).name;
            field->inferred_type = false;
        } else if (relationship_key_type(declaration, key, origin) != value_type) {
            report(origin, "Relationship foreign and local key types disagree for '" + key + "'", "GNR2310");
        }
    }
    void index_relationships(std::size_t d) {
        const auto& declaration = v.syntax_.declarations[d];
        current_module = declaration.module;
        current_owner = v.declarations_[d].symbol;
        for (const auto& relation : declaration.relationships) {
            RelationshipResolution resolved;
            const bool through = relation.kind == "hasOneThrough" || relation.kind == "hasManyThrough";
            if (relation.types.size() != (through ? 2 : 1)) {
                report(relation.origin, "Relationship requires a related model" +
                    std::string(through ? " and an intermediate model" : ""), "GNR2310");
                continue;
            }
            const auto model_type = [&](const TypeSyntax& syntax) {
                const auto value = resolve(syntax);
                const auto owner = owner_of(value);
                if (type(value).optional || owner == invalid_id ||
                    v.syntax_.declarations[declaration_ids.at(owner)].kind != DeclarationKind::model) {
                    report(syntax.origin, "Relationships require non-optional model types", "GNR2310");
                    return invalid_id;
                }
                return value;
            };
            resolved.related = model_type(relation.types[0]);
            if (through) resolved.through = model_type(relation.types[1]);
            if (resolved.related == invalid_id || (through && resolved.through == invalid_id)) continue;
            const auto related = declaration_ids.at(owner_of(resolved.related));
            const auto intermediate = through ? declaration_ids.at(owner_of(resolved.through)) : invalid_id;
            const auto parent_key = primary_key(d), related_key = primary_key(related);
            std::vector<std::string> names;
            if (relation.kind == "belongsToMany") {
                auto parent_name = foreign_key(declaration, ""); parent_name.pop_back();
                auto related_name = foreign_key(v.syntax_.declarations[related], ""); related_name.pop_back();
                names = {"pivotTable", "foreignPivotKey", "relatedPivotKey", "parentKey", "relatedKey"};
                resolved.keys = {std::min(parent_name, related_name) + "_" + std::max(parent_name, related_name),
                    foreign_key(declaration, parent_key), foreign_key(v.syntax_.declarations[related], related_key),
                    parent_key, related_key};
            } else if (through) {
                names = {"firstKey", "secondKey", "localKey", "secondLocalKey"};
                resolved.keys = {foreign_key(declaration, parent_key),
                    foreign_key(v.syntax_.declarations[intermediate], primary_key(intermediate)),
                    parent_key, primary_key(intermediate)};
            } else {
                names = {"foreignKey", relation.kind == "belongsTo" ? "ownerKey" : "localKey"};
                resolved.keys = {relation.kind == "belongsTo" ? relation.name + "_" + related_key : foreign_key(declaration, parent_key),
                    relation.kind == "belongsTo" ? related_key : parent_key};
            }
            std::unordered_set<std::size_t> supplied;
            bool named = false;
            std::size_t position = 0;
            for (std::size_t a = 0; a < relation.arguments.size(); ++a) {
                const auto& argument = v.syntax_.expressions[relation.arguments[a]];
                std::size_t target = position++;
                if (!relation.argument_names[a].empty()) {
                    named = true;
                    const auto found = std::find(names.begin(), names.end(), relation.argument_names[a]);
                    target = static_cast<std::size_t>(found - names.begin());
                } else if (named) report(argument.origin, "Positional relationship keys must precede named keys", "GNR2310");
                if (target >= names.size() || !supplied.insert(target).second) {
                    report(argument.origin, "Unknown, duplicate, or excess relationship key", "GNR2310");
                    continue;
                }
                if (argument.literal_type != "string" || !valid_identifier(argument.text)) {
                    report(argument.origin, "Relationship keys must be constant attribute/table identifiers", "GNR2310");
                    continue;
                }
                v.expressions_[relation.arguments[a]].type = type_id("string");
                resolved.keys[target] = argument.text;
            }
            const auto& keys = resolved.keys;
            if (relation.kind == "belongsToMany") {
                relationship_key_type(d, keys[3], relation.origin);
                relationship_key_type(related, keys[4], relation.origin);
            } else if (through) {
                relationship_foreign_key(intermediate, keys[0], relationship_key_type(d, keys[2], relation.origin), relation.origin);
                relationship_foreign_key(related, keys[1], relationship_key_type(intermediate, keys[3], relation.origin), relation.origin);
            } else if (relation.kind == "belongsTo") {
                relationship_foreign_key(d, keys[0], relationship_key_type(related, keys[1], relation.origin), relation.origin);
            } else {
                relationship_foreign_key(related, keys[0], relationship_key_type(d, keys[1], relation.origin), relation.origin);
            }
            v.declarations_[d].relationships.push_back(std::move(resolved));
        }
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
            auto& declaration = v.syntax_.declarations[d];
            // Metadata-only models still expose conventional typed attributes.
            // Lifecycle metadata also implies concrete ORM fields so the
            // generated model and the runtime contract cannot drift apart.
            if (declaration.kind == DeclarationKind::model) {
                std::set<std::string> attributes;
                std::unordered_map<std::string, std::string> casts;
                bool timestamps = false;
                bool soft_deletes = false;
                std::string primary = "id";

                for (const auto& metadata : declaration.metadata) {
                    const auto& expression =
                        v.syntax_.expressions[metadata.value];

                    if (
                        metadata.name == "fillable" ||
                        metadata.name == "hidden" ||
                        metadata.name == "visible"
                    ) {
                        for (auto id : expression.operands) {
                            const auto& item =
                                v.syntax_.expressions[id];

                            if (item.literal_type == "string") {
                                attributes.insert(item.text);
                            }
                        }
                    }

                    if (metadata.name == "casts") {
                        for (
                            std::size_t i = 0;
                            i < expression.argument_names.size() &&
                            i < expression.operands.size();
                            ++i
                        ) {
                            const auto& value =
                                v.syntax_.expressions[
                                    expression.operands[i]
                                ];

                            attributes.insert(
                                expression.argument_names[i]
                            );

                            if (value.literal_type == "string") {
                                casts.insert_or_assign(
                                    expression.argument_names[i],
                                    value.text
                                );
                            }
                        }
                    }

                    if (
                        metadata.name == "primaryKey" &&
                        expression.literal_type == "string"
                    ) {
                        primary = expression.text;
                    }

                    if (
                        metadata.name == "timestamps" &&
                        expression.literal_type == "bool" &&
                        expression.text == "true"
                    ) {
                        timestamps = true;
                    }

                    if (
                        metadata.name == "softDeletes" &&
                        expression.literal_type == "bool" &&
                        expression.text == "true"
                    ) {
                        soft_deletes = true;
                    }
                }

                attributes.insert(primary);

                if (timestamps) {
                    attributes.insert("created_at");
                    attributes.insert("updated_at");
                }

                if (soft_deletes) {
                    attributes.insert("deleted_at");
                }

                const auto cast_type = [&](const std::string& name) {
                    const auto found = casts.find(name);

                    if (found == casts.end()) {
                        return name == primary
                            ? std::string{"int"}
                            : std::string{"string"};
                    }

                    if (
                        found->second == "int" ||
                        found->second == "integer"
                    ) {
                        return std::string{"int"};
                    }

                    if (found->second == "bool") {
                        return std::string{"bool"};
                    }

                    if (found->second == "double") {
                        return std::string{"double"};
                    }

                    if (found->second == "decimal") {
                        return std::string{"Decimal"};
                    }

                    if (found->second == "json") {
                        return std::string{"Json"};
                    }

                    return std::string{"string"};
                };

                for (const auto& name : attributes) {
                    if (
                        std::any_of(
                            declaration.fields.begin(),
                            declaration.fields.end(),
                            [&](const auto& field) {
                                return field.name == name;
                            }
                        )
                    ) {
                        continue;
                    }

                    const bool lifecycle_nullable =
                        (timestamps &&
                         (name == "created_at" ||
                          name == "updated_at")) ||
                        (soft_deletes &&
                         name == "deleted_at");

                    declaration.fields.push_back(
                        FieldSyntax{
                            declaration.origin,
                            name,
                            TypeSyntax{
                                cast_type(name),
                                {},
                                lifecycle_nullable,
                                declaration.origin
                            },
                            false, invalid_id, Visibility::public_,
                            name != primary && !casts.contains(name) && !lifecycle_nullable
                        }
                    );
                }
            }
        }
        for (std::size_t d = 0; d < v.syntax_.declarations.size(); ++d) index_relationships(d);
        if (!diagnostics.empty()) return;
        for (std::size_t d = 0; d < v.syntax_.declarations.size(); ++d) {
            auto& declaration = v.syntax_.declarations[d]; auto& resolved = v.declarations_[d]; current_module = declaration.module; current_owner = resolved.symbol;
            for (const auto& field : declaration.fields) {
                if (resolve(field.type) == type_id("void")) report(field.origin,"Fields cannot have type void");
                if (!valid_identifier(field.name)) report(field.origin,"Field name is not a valid native identifier");
                const auto id = symbol({field.injection ? ResolvedSymbolKind::injection : ResolvedSymbolKind::field, field.name, field.name, resolve(field.type), current_owner,
                    declaration.kind == DeclarationKind::event || declaration.kind == DeclarationKind::mail || declaration.kind == DeclarationKind::notification || declaration.kind == DeclarationKind::job, false, field.visibility});
                if (!members.emplace(std::to_string(current_owner) + ":" + field.name, id).second) report(field.origin, "Duplicate field '" + field.name + "'"); resolved.fields.push_back(id);
            }
            for (std::size_t r = 0; r < declaration.relationships.size(); ++r) {
                const auto& relation = declaration.relationships[r];
                auto& resolved_relation = resolved.relationships[r];
                const auto cpp = "gungnir::" + relationship_type(relation.kind) + "<" +
                    type(resolved_relation.related).cpp_name +
                    (resolved_relation.through == invalid_id ? "" : "," + type(resolved_relation.through).cpp_name) + ">";
                std::vector<TypeId> arguments{resolved_relation.related};
                if (resolved_relation.through != invalid_id) arguments.push_back(resolved_relation.through);
                const auto relation_type = intern(relationship_type(relation.kind), cpp, arguments);
                const auto field = symbol({ResolvedSymbolKind::field, relation.name, relation.name,
                    relation_type, current_owner});
                resolved_relation.field = field;
                if (!valid_identifier(relation.name) ||
                    !members.emplace(std::to_string(current_owner) + ":" + relation.name, field).second)
                    report(relation.origin, "Relationship conflicts with a model member: " + relation.name, "GNR2310");
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
            const auto common = common_type(yes, no);
            if (!common) {
                report(e.origin, "Conditional branches have incompatible types");
                return set(type_id("Value"));
            }
            return set(*common);
        }
        const auto lhs = expression(e.operands[0]);
        if (e.text == "??") {
            std::function<bool(SyntaxId)> contains_await = [&](SyntaxId id) { const auto& node = v.syntax_.expressions[id]; if (node.kind == SyntaxExpressionKind::await_) return true; if (node.kind == SyntaxExpressionKind::lambda) return false; for (auto child : node.operands) if (contains_await(child)) return true; return false; };
            if (contains_await(e.operands[0]) || contains_await(e.operands[1])) report(e.origin,"await in null coalescing requires a separate binding"); if (!type(lhs).optional) report(e.origin, "Null coalescing requires an optional value"); const auto base = unoptional(lhs); if (!assignable(base, expression(e.operands[1], base))) report(e.origin, "Incompatible coalescing value"); return set(base); }
        TypeId assignment_target = lhs;
        if (e.text == "=") {
            const auto target_symbol = v.expressions_[e.operands[0]].symbol;
            const auto& target_syntax = v.syntax_.expressions[e.operands[0]];
            if (target_symbol != invalid_id &&
                (target_syntax.kind == SyntaxExpressionKind::name ||
                 target_syntax.kind == SyntaxExpressionKind::member)) {
                assignment_target = v.symbols_[target_symbol].type;
            }
        }
        const auto rhs = expression(e.operands[1], e.text == "=" ? assignment_target : lhs);
        if (e.text == "=" || e.text == "+=" || e.text == "-=" || e.text == "*=" || e.text == "/=") {
            writable(e.operands[0]);
            if (e.text != "=" && !(numeric(lhs) && numeric(rhs)) && !(e.text == "+=" && lhs == type_id("string") && rhs == lhs))
                report(e.origin,"Compound assignment requires compatible numeric or string operands");
            if (!assignable(e.text == "=" ? assignment_target : lhs, rhs)) report(e.origin, "Assignment type mismatch");
            return set(e.text == "=" ? assignment_target : lhs);
        }
        if (e.text == "&&" || e.text == "||") { if (lhs != type_id("bool") || rhs != type_id("bool")) report(e.origin, "Logical operands must be bool"); return set(type_id("bool")); }
        if (e.text == "==" || e.text == "!=") { if (!(numeric(unoptional(lhs)) && numeric(unoptional(rhs))) && unoptional(lhs) != type_id("string") && unoptional(lhs) != type_id("bool") && type(lhs).name != "null" && type(rhs).name != "null") report(e.origin,"Equality is supported for scalar and optional scalar values"); if (!assignable(lhs, rhs) && !assignable(rhs, lhs)) report(e.origin, "Incompatible equality operands"); return set(type_id("bool")); }
        if (e.text == "<" || e.text == ">" || e.text == "<=" || e.text == ">=") { if (!(numeric(lhs) && numeric(rhs)) && !(lhs == type_id("string") && rhs == lhs)) report(e.origin, "Incompatible comparison operands"); return set(type_id("bool")); }
        if (e.text == "+" && lhs == type_id("string") && rhs == lhs) return set(lhs);
        if (!numeric(lhs) || !numeric(rhs)) {
            report(e.origin, "Arithmetic operands must be numbers");
            return set(type_id("Value"));
        }
        if (e.text == "%" && (lhs != type_id("int") || rhs != lhs)) report(e.origin, "Remainder operands must be integers");
        const auto common = common_type(lhs, rhs);
        if (!common || !numeric(*common)) {
            report(e.origin, "Mixed numeric operands require an explicit conversion");
            return set(type_id("Value"));
        }
        return set(*common);
    }
    void writable(SyntaxId id) {
        const auto& expression = v.syntax_.expressions[id]; const auto symbol = v.expressions_[id].symbol;
        if (expression.kind != SyntaxExpressionKind::name && expression.kind != SyntaxExpressionKind::member && expression.kind != SyntaxExpressionKind::subscript) report(expression.origin, "Assignment requires a writable target");
        if (symbol != invalid_id && v.symbols_[symbol].immutable) report(expression.origin, "Cannot modify an immutable binding", "GNR2208");
        if (expression.kind == SyntaxExpressionKind::name && symbol != invalid_id) narrowed.erase(symbol);
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
        auto inferred = returned.empty() ? type_id("void") : returned.front();
        for (std::size_t i = 1; i < returned.size(); ++i) {
            const auto common = common_type(inferred, returned[i]);
            if (!common) {
                report(e.origin, "Lambda returns incompatible types");
                inferred = type_id("Value");
                break;
            }
            inferred = *common;
        }
        if (inferred != type_id("void") && !block_flow(e.body).terminates_path()) report(e.origin, "Lambda may finish without returning a value");
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
        info.argument_order.clear(); info.argument_conversions.clear();
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
            for (std::size_t i = 0; i < count; ++i) {
                const auto argument = e.operands[i + 1];
                const auto context = i < contexts.size() ? contexts[i] : std::nullopt;
                const auto cached = v.expressions_[argument].type;
                argument_types.push_back(cached != invalid_id && (!context || *context == cached || type(*context).name != "Function")
                    ? cached : expression(argument, context));
                if (context && type(*context).name != "Function" && !assignable(*context, argument_types.back()))
                    report(e.origin, "Builtin argument type mismatch");
            }
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
            if (name == "noContent") { arity(0,0); return finish_builtin(type_id("Response"), "gungnir::language::runtime::no_content"); }
            if (name == "download") { arity(2,4); return finish_builtin(type_id("Response"), "gungnir::language::runtime::download", {type_id("string"),type_id("string"),type_id("string"),type_id("int")}); }
            if (name == "text" || name == "html" || name == "json" || name == "view" || name == "redirect" || name == "response") { arity(1, name == "view" ? 3 : 2); return finish_builtin(type_id("Response"), "gungnir::language::runtime::" + name,
                name == "view" ? std::vector<std::optional<TypeId>>{type_id("string"),type_id("Json"),type_id("int")} : std::vector<std::optional<TypeId>>{name == "json" ? type_id("Json") : type_id("string"),type_id("int")}); }
            if (name == "exactDecimal") { arity(1,1); return finish_builtin(type_id("Decimal"), "gungnir::model::Decimal", {type_id("string")}); }
            if (name == "authorize") { arity(3,3); return finish_builtin(type_id("void"), "gungnir::language::runtime::authorize", {type_id("Request"),type_id("string"),std::nullopt}); }
            if (name == "allow" || name == "deny") { arity(0, name == "allow" ? 0 : 1); return finish_builtin(type_id("Decision"), "gungnir::auth::Decision::" + name, {type_id("string")}); }
        }
        if (receiver != invalid_id && (callable_id == invalid_id || v.symbols_[callable_id].kind == ResolvedSymbolKind::field)) {
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
                if (name == "json") { arity(0,0); return finish_builtin(type_id("Json"), name); }
                if (name == "validate") { arity(1,1); return finish_builtin(type_id("Json"), "gungnir::language::runtime::validate", {type_id("Json")}); }
                const auto input_map = intern("Map", "std::unordered_map<gungnir::String,gungnir::String>", {type_id("string"),type_id("string")});
                if (name == "query") { arity(0,1); return finish_builtin(count ? type_id("string") : input_map, name, {type_id("string")}); }
                if (name == "all" || name == "headers" || name == "parameters" || name == "cookies" || name == "form") { arity(0,0); return finish_builtin(input_map, name); }
                if (name == "only" || name == "except") { arity(1,1); return finish_builtin(input_map, name, {sequence("List",type_id("string"))}); }
                if (name == "method") { arity(0,0); return finish_builtin(type_id("string"), "method_name"); }
                static const std::unordered_set<std::string> keyed_strings{"header","parameter","input","cookie"};
                if (keyed_strings.contains(name)) { arity(1,1); return finish_builtin(type_id("string"), name, {type_id("string")}); }
                static const std::unordered_set<std::string> strings{"body","path","target","contentType","userAgent","host","authorization","bearerToken","clientIp"};
                if (strings.contains(name)) { arity(0,0); return finish_builtin(type_id("string"), snake(name)); }
                static const std::unordered_set<std::string> states{"cancelled","secure","hasServices","hasSession","hasAuth","authenticated","guest","expectsJson","isJson"};
                if (states.contains(name)) { arity(0,0); return finish_builtin(type_id("bool"), snake(name)); }
                if (name == "has" || name == "hasParameter" || name == "accepts") { arity(1,1); return finish_builtin(type_id("bool"), snake(name), {type_id("string")}); }
            }
            if (callable_id == invalid_id && t.name == "Response") {
                const bool mutation = (name == "header" && count == 2) ||
                    ((name == "status" || name == "body") && count == 1);
                if (mutation && receiver_expression != invalid_id) {
                    const auto kind = v.syntax_.expressions[receiver_expression].kind;
                    if (kind == SyntaxExpressionKind::name || kind == SyntaxExpressionKind::member ||
                        kind == SyntaxExpressionKind::subscript) writable(receiver_expression);
                }
                if (name == "header") { arity(1,2); return finish_builtin(type_id(count == 1 ? "string" : "Response"), name, {type_id("string"),type_id("string")}); }
                if (name == "status") { arity(0,1); return finish_builtin(type_id(count ? "Response" : "int"), name, {type_id("int")}); }
                if (name == "body") { arity(0,1); return finish_builtin(type_id(count ? "Response" : "string"), name, {type_id("string")}); }
                if (name == "headers") { arity(0,0); return finish_builtin(intern("Map", "std::unordered_map<gungnir::String,gungnir::String>", {type_id("string"),type_id("string")}), name); }
            }
            if (callable_id == invalid_id && (t.name == "Json" || t.name == "Value" || t.name == "Data")) {
                if (name == "string" || name == "dump") { arity(0,0); return finish_builtin(type_id("string"), name); }
                if (name == "get") { arity(1,1); return finish_builtin(optional(type_id("Json")), "find", {type_id("string")}); }
                if (name == "asArray") { arity(0,0); return finish_builtin(sequence("List",type_id("Json")), "as_array"); }
                if (name == "asObject") { arity(0,0); return finish_builtin(intern("Map", "std::unordered_map<gungnir::String,gungnir::Json>", {type_id("string"),type_id("Json")}), "as_object"); }
                static const std::unordered_set<std::string> states{"isNull","isBoolean","isInteger","isNumber","isString","isArray","isObject"};
                if (states.contains(name)) { arity(0,0); return finish_builtin(type_id("bool"), snake(name)); }
            }
            const auto owner = owner_of(receiver);
            const bool model = owner != invalid_id && v.syntax_.declarations[declaration_ids.at(owner)].kind == DeclarationKind::model;
            if (model && callable_id != invalid_id) {
                const auto& relations = v.declarations_[declaration_ids.at(owner)].relationships;
                const auto found = std::find_if(relations.begin(), relations.end(),
                    [&](const auto& relation) { return relation.field == callable_id; });
                if (found != relations.end()) {
                    arity(0, 0);
                    if (callee.literal_type == "::") report(callee.origin, "Relationship queries require an instance receiver", "GNR2311");
                    const auto cpp = "gungnir::language::runtime::relationship_query<&" +
                        type(unoptional(receiver)).cpp_name + "::" + name + ">";
                    const auto result = finish_builtin(sequence("Query", found->related), cpp);
                    v.symbols_[info.symbol].receives_receiver = true;
                    v.symbols_[info.symbol].owner = found->field;
                    return result;
                }
            }
            const bool relation_type = t.name == "HasOne" || t.name == "HasMany" ||
                t.name == "BelongsTo" || t.name == "BelongsToMany" ||
                t.name == "HasOneThrough" || t.name == "HasManyThrough";
            if (callable_id == invalid_id && relation_type) {
                const auto related = t.arguments[0];
                const bool many = t.name == "HasMany" || t.name == "BelongsToMany" || t.name == "HasManyThrough";
                if (name == "loaded" || name == "empty") { arity(0, 0); return finish_builtin(type_id("bool"), name); }
                if (name == "unload") { arity(0, 0); return finish_builtin(type_id("void"), name); }
                if (name == "get") { arity(0, 0); return finish_builtin(many ? sequence("List", related) : related, name); }
                if (many && (name == "size" || name == "count")) { arity(0, 0); return finish_builtin(type_id("int"), "size"); }
                if (!many && name == "value") {
                    arity(0, 0);
                    const auto result = finish_builtin(optional(related), "gungnir::language::runtime::relationship_value");
                    v.symbols_[info.symbol].receives_receiver = true;
                    return result;
                }
                if (t.name == "BelongsToMany" && (name == "attach" || name == "detach")) {
                    arity(name == "attach" ? 1 : 0, 1);
                    const auto& field = v.syntax_.expressions[receiver_expression];
                    if (field.kind != SyntaxExpressionKind::member || field.operands.empty()) {
                        report(e.origin, "Pivot mutations require a model relationship access", "GNR2311");
                        return finish_builtin(type_id("int"), name);
                    }
                    const auto parent = v.expressions_[field.operands[0]].type;
                    const auto parent_symbol = v.expressions_[field.operands[0]].symbol;
                    if (parent_symbol != invalid_id && v.symbols_[parent_symbol].immutable)
                        report(e.origin, "Cannot modify an immutable binding", "GNR2208");
                    const auto parent_declaration = declaration_ids.at(owner_of(parent));
                    const auto& relations = v.declarations_[parent_declaration].relationships;
                    const auto relation = std::find_if(relations.begin(), relations.end(), [&](const auto& value) {
                        return value.field == v.expressions_[receiver_expression].symbol;
                    });
                    const auto related_declaration = declaration_ids.at(owner_of(related));
                    const auto key = relationship_key_type(related_declaration,
                        relation == relations.end() ? primary_key(related_declaration) : relation->keys[4], e.origin);
                    const bool list = count && (v.syntax_.expressions[e.operands[1]].kind == SyntaxExpressionKind::list ||
                        type(expression(e.operands[1])).name == "List");
                    const auto result = finish_builtin(type_id("int"), "gungnir::language::runtime::relationship_" + name +
                        "<&" + type(unoptional(parent)).cpp_name + "::" + field.text + ">",
                        list
                            ? std::vector<std::optional<TypeId>>{sequence("List", key)}
                            : std::vector<std::optional<TypeId>>{key});
                    v.symbols_[info.symbol].receives_receiver = true;
                    info.receiver_expression = field.operands[0];
                    return result;
                }
            }
            if (callable_id == invalid_id && (model || t.name == "Query")) {
                const auto model_type = model ? unoptional(receiver) : t.arguments[0];
                const auto model_declaration = declaration_ids.at(owner_of(model_type));
                const auto query_type = sequence("Query", model_type);
                const auto query_cpp = [&](const std::string& method) { return model ? "query()." + method : method; };
                const auto scalar_argument = [&](std::size_t index) {
                    if (index >= count) return;
                    const auto value = unoptional(expression(e.operands[index + 1]));
                    const auto& value_name = type(value).name;
                    if (!numeric(value) && value_name != "Decimal" && value_name != "string" &&
                        value_name != "bool" && value_name != "null")
                        report(e.origin, "ORM values require scalar or optional scalar attributes", "GNR2311");
                };
                if (name == "all" || name == "get") {
                    arity(0, 0); return finish_builtin(sequence("Collection", model_type), query_cpp("get"));
                }
                if (name == "first" || name == "firstOrFail") {
                    arity(0, 0); return finish_builtin(name == "first" ? optional(model_type) : model_type, query_cpp(snake(name)));
                }
                if (name == "find" || name == "findOrFail") {
                    arity(1, 1);
                    const auto key = relationship_key_type(model_declaration, primary_key(model_declaration), e.origin);
                    const auto result = finish_builtin(name == "find" ? optional(model_type) : model_type,
                        model ? snake(name) : "gungnir::language::runtime::query_" + snake(name), {key});
                    if (!model) v.symbols_[info.symbol].receives_receiver = true;
                    return result;
                }
                if (model && name == "create") { arity(1, 1); return finish_builtin(model_type, name, {type_id("Json")}); }
                if (name == "count" || (!model && name == "exists")) {
                    arity(0, 0); return finish_builtin(type_id(name == "exists" ? "bool" : "int"), query_cpp(name));
                }
                if (name == "paginate") {
                    arity(0, 2); return finish_builtin(sequence("Page", model_type), query_cpp(name), {type_id("int"), type_id("int")});
                }
                static const std::unordered_set<std::string> mutations{"save", "remove", "forceRemove", "update", "restore", "touch", "refresh"};
                if (mutations.contains(name) && (model || (name != "save" && name != "touch" && name != "refresh"))) {
                    arity(name == "update" ? 1 : 0, name == "update" ? 1 : 0);
                    if (model && callee.literal_type == "::") report(callee.origin, "Model mutations require an instance receiver", "GNR2311");
                    if (model && v.expressions_[receiver_expression].symbol != invalid_id &&
                        v.symbols_[v.expressions_[receiver_expression].symbol].immutable)
                        report(callee.origin, "Cannot modify an immutable binding", "GNR2208");
                    return finish_builtin(type_id(model ? "bool" : "int"), snake(name), {type_id("Json")});
                }
                if (model && (name == "dirty" || name == "exists" || name == "trashed" || name == "relationLoaded" || name == "isDirty")) {
                    if (callee.literal_type == "::") report(callee.origin, "Model state methods require an instance receiver", "GNR2311");
                    const bool argument = name == "relationLoaded" || name == "isDirty";
                    arity(argument ? 1 : 0, argument ? 1 : 0);
                    return finish_builtin(type_id("bool"), snake(name), {type_id("string")});
                }
                if (model && name == "unloadRelations") { arity(0, 0); return finish_builtin(type_id("void"), "unload_relations"); }
                if (name == "query") { arity(0, 0); if (model) return finish_builtin(query_type, name); }
                if (name == "where" || name == "orWhere") {
                    arity(2, 3); scalar_argument(count == 3 ? 2 : 1);
                    if (count == 3) {
                        const auto& comparison = v.syntax_.expressions[e.operands[2]];
                        static const std::unordered_set<std::string> comparisons{"=", "==", "!=", "<>", "<", "<=", ">", ">=", "like"};
                        if (comparison.literal_type == "string" && !comparisons.contains(comparison.text))
                            report(comparison.origin, "Unsupported ORM comparison", "GNR2311");
                    }
                    return finish_builtin(query_type, query_cpp(snake(name)), count == 3
                        ? std::vector<std::optional<TypeId>>{type_id("string"), type_id("string"), std::nullopt}
                        : std::vector<std::optional<TypeId>>{type_id("string"), std::nullopt});
                }
                if (name == "whereIn" || name == "whereNotIn") {
                    arity(2, 2);
                    if (count > 1) {
                        const auto& argument = v.syntax_.expressions[e.operands[2]];
                        const auto values = expression(e.operands[2], argument.kind == SyntaxExpressionKind::list && argument.operands.empty()
                            ? std::optional<TypeId>{sequence("List", type_id("int"))} : std::nullopt);
                        if (type(values).name != "List" || type(values).arguments.empty()) report(e.origin, "whereIn requires a list of scalar attributes", "GNR2311");
                        else {
                            const auto element = unoptional(type(values).arguments[0]);
                            const auto& element_name = type(element).name;
                            if (!numeric(element) && element_name != "string" && element_name != "bool" && element_name != "Decimal" && element_name != "null")
                                report(e.origin, "whereIn requires a list of scalar attributes", "GNR2311");
                        }
                    }
                    return finish_builtin(query_type, query_cpp(snake(name)), {type_id("string"), std::nullopt});
                }
                if (name == "orderBy" || name == "orderByDesc" || name == "latest" || name == "oldest") {
                    arity(name == "latest" || name == "oldest" ? 0 : 1, name == "orderBy" ? 2 : 1);
                    if (name == "orderBy" && count == 2) {
                        const auto& direction = v.syntax_.expressions[e.operands[2]];
                        if (direction.literal_type == "string" && direction.text != "asc" && direction.text != "desc")
                            report(direction.origin, "Order direction must be asc or desc", "GNR2311");
                    }
                    return finish_builtin(query_type, query_cpp(snake(name)), {type_id("string"), type_id("string")});
                }
                if (name == "limit" || name == "offset" || name == "take" || name == "skip") {
                    arity(1, 1); return finish_builtin(query_type, query_cpp(name), {type_id("int")});
                }
                if (name == "with") {
                    arity(1, 1);
                    const bool list = count && (v.syntax_.expressions[e.operands[1]].kind == SyntaxExpressionKind::list ||
                        type(expression(e.operands[1])).name == "List");
                    return finish_builtin(query_type, query_cpp(name), {list
                        ? sequence("List", type_id("string")) : type_id("string")});
                }
                if (name == "select") { arity(1, 1); return finish_builtin(query_type, query_cpp(name), {sequence("List", type_id("string"))}); }
                if (name == "withDeleted" || name == "onlyDeleted") { arity(0, 0); return finish_builtin(query_type, query_cpp(snake(name))); }
            }
            if (callable_id == invalid_id && t.name == "Page" && (name == "empty" || name == "hasMore" || name == "hasPrevious")) {
                arity(0, 0); return finish_builtin(type_id("bool"), snake(name));
            }
            if (callable_id == invalid_id && (t.name == "List" || t.name == "Collection")) {
                const auto element = t.arguments[0];
                if (name == "count" || name == "size") { arity(0, 0); return finish_builtin(type_id("int"), "size"); }
                if (name == "empty" || name == "isEmpty") { arity(0, 0); return finish_builtin(type_id("bool"), "empty"); }
                if (t.name == "Collection") {
                    if (name == "first" || name == "last") { arity(0, 0); return finish_builtin(element, name); }
                    if (name == "at") { arity(1, 1); return finish_builtin(element, name, {type_id("int")}); }
                    if (name == "values") { arity(0, 0); return finish_builtin(sequence("List", element), name); }
                    if (name == "find") {
                        arity(1, 1);
                        const auto element_owner = owner_of(element);
                        if (element_owner == invalid_id || v.syntax_.declarations[declaration_ids.at(element_owner)].kind != DeclarationKind::model) {
                            report(e.origin, "Collection find requires model elements", "GNR2311");
                            return finish_builtin(optional(element), name);
                        }
                        const auto declaration = declaration_ids.at(element_owner);
                        return finish_builtin(optional(element), name, {relationship_key_type(declaration, primary_key(declaration), e.origin)});
                    }
                    if (name == "take" || name == "skip" || name == "chunk") {
                        arity(1, 1); return finish_builtin(name == "chunk" ? sequence("List", receiver) : receiver, name, {type_id("int")});
                    }
                    if (name == "contains" || name == "every" || name == "reject") {
                        arity(1, 1);
                        return finish_builtin(name == "reject" ? receiver : type_id("bool"), name,
                            {intern("Function", "auto", {element, type_id("bool")})});
                    }
                    if (name == "sortBy" || name == "unique" || name == "sum") {
                        arity(1, name == "sortBy" ? 2 : 1);
                        const auto callback = count ? expression(e.operands[1], intern("Function", "auto", {element, type_id("Value")})) : type_id("Callable");
                        const auto projected = type(callback).arguments.empty() ? type_id("Value") : type(callback).arguments.back();
                        if ((name == "sum" && !numeric(projected)) || (name != "sum" && !numeric(projected) && projected != type_id("string") && projected != type_id("bool")))
                            report(e.origin, "Collection projection must return " + std::string(name == "sum" ? "a number" : "a comparable scalar"), "GNR2311");
                        return finish_builtin(name == "sum" ? projected : receiver, snake(name), {callback, type_id("bool")});
                    }
                    if (name == "reduce") {
                        arity(2, 2);
                        const auto initial = count ? expression(e.operands[1]) : type_id("Value");
                        return finish_builtin(initial, name, {initial, intern("Function", "auto", {initial, element, initial})});
                    }
                }
                if (name == "map" || name == "filter" || name == "each") {
                    arity(1, 1); const auto callback = count ? expression(e.operands[1], intern("Function", "auto", {element, type_id("Value")})) : type_id("Callable");
                    if (name == "filter" && (type(callback).arguments.empty() || type(callback).arguments.back() != type_id("bool"))) report(e.origin,"filter callback must return bool");
                    if (name == "map" && !type(callback).arguments.empty() && type(callback).arguments.back() == type_id("void")) report(e.origin,"map callback must return a value; use each for void callbacks");
                    const bool collection_filter = name == "filter" && t.name == "Collection";
                    auto result = name == "each" ? type_id("void") : sequence(collection_filter ? "Collection" : "List", name == "map" && !type(callback).arguments.empty() ? type(callback).arguments.back() : element);
                    callable_id = builtin(name, collection_filter ? name : "gungnir::language::runtime::" + name, result, {callback}); info.type = result; info.symbol = callable_id; info.argument_order = {0}; info.argument_conversions = {callback}; v.expressions_[e.operands[0]] = {type_id("Callable"), callable_id}; return result;
                }
            }
            if (callable_id == invalid_id && t.name == "string" && (name == "size" || name == "length" || name == "empty")) { arity(0,0); return finish_builtin(type_id(name == "empty" ? "bool" : "int"), name == "length" ? "size" : name); }
        }
        if (callable_id != invalid_id && type(v.symbols_[callable_id].type).name == "Next") {
            const auto binding = v.symbols_[callable_id];
            ResolvedSymbol callable{ResolvedSymbolKind::callable,binding.name,binding.cpp_name,type_id("Response"),callable_id,true,true};
            callable.parameters = {type_id("Request")}; callable.parameter_names = {"request"}; callable.defaults = {invalid_id};
            callable_id = symbol(std::move(callable));
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
    struct FlowResult {
        bool falls_through{true};
        bool returns{false};
        bool throws{false};
        bool breaks{false};
        bool continues{false};

        [[nodiscard]] bool terminates_path() const noexcept {
            return !falls_through;
        }
    };

    FlowResult statement_flow(SyntaxId id) const {
        const auto& statement = v.syntax_.statements[id];
        switch (statement.kind) {
        case SyntaxStatementKind::return_:
            return {false, true, false, false, false};
        case SyntaxStatementKind::throw_:
            return {false, false, true, false, false};
        case SyntaxStatementKind::break_:
            return {false, false, false, true, false};
        case SyntaxStatementKind::continue_:
            return {false, false, false, false, true};
        case SyntaxStatementKind::block:
            return block_flow(statement.body);
        case SyntaxStatementKind::if_: {
            const auto body = block_flow(statement.body);
            const auto alternative = statement.alternative.empty()
                ? FlowResult{}
                : block_flow(statement.alternative);
            return {
                body.falls_through || alternative.falls_through,
                body.returns || alternative.returns,
                body.throws || alternative.throws,
                body.breaks || alternative.breaks,
                body.continues || alternative.continues
            };
        }
        case SyntaxStatementKind::while_:
        case SyntaxStatementKind::for_:
        case SyntaxStatementKind::for_in: {
            // Loops are conservatively assumed to be able to finish. Return
            // and throw paths remain visible to callers, while loop-local
            // break/continue are consumed by the loop itself.
            const auto body = block_flow(statement.body);
            return {true, body.returns, body.throws, false, false};
        }
        default:
            return {};
        }
    }

    FlowResult block_flow(const std::vector<SyntaxId>& body) const {
        FlowResult result;
        for (const auto id : body) {
            if (!result.falls_through) break;
            const auto next = statement_flow(id);
            result.returns = result.returns || next.returns;
            result.throws = result.throws || next.throws;
            result.breaks = result.breaks || next.breaks;
            result.continues = result.continues || next.continues;
            result.falls_through = next.falls_through;
        }
        return result;
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
                const bool body_returns = s.kind == SyntaxStatementKind::if_ && block_flow(s.body).terminates_path();
                const bool alternative_returns = s.kind == SyntaxStatementKind::if_ && !s.alternative.empty() && block_flow(s.alternative).terminates_path();
                if (guarded != invalid_id && nonnull) narrowed.insert(guarded);
                scopes.emplace_back(); if (s.kind == SyntaxStatementKind::while_) ++loops; statements(s.body); if (s.kind == SyntaxStatementKind::while_) --loops; scopes.pop_back();
                const bool body_keeps_nonnull = guarded != invalid_id && narrowed.contains(guarded);
                narrowed = previous; if (guarded != invalid_id && !nonnull) narrowed.insert(guarded);
                scopes.emplace_back(); statements(s.alternative); scopes.pop_back();
                const bool alternative_keeps_nonnull = guarded != invalid_id && narrowed.contains(guarded);
                narrowed = previous;
                if (s.kind == SyntaxStatementKind::if_ && guarded != invalid_id && body_returns != alternative_returns) {
                    const bool continuation_keeps_nonnull = alternative_returns ? body_keeps_nonnull : alternative_keeps_nonnull;
                    if (continuation_keeps_nonnull) narrowed.insert(guarded);
                }
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
        auto& declaration = v.syntax_.declarations[d];
        auto& resolved = v.declarations_[d];

        const auto method_index = [&](std::string_view name) {
            const auto found = std::find_if(
                declaration.methods.begin(),
                declaration.methods.end(),
                [&](const auto& method) {
                    return method.name == name;
                }
            );

            return found == declaration.methods.end()
                ? invalid_id
                : static_cast<std::size_t>(
                    found - declaration.methods.begin()
                );
        };

        const auto is_model_type = [&](TypeId id) {
            if (id == invalid_id || type(id).optional) {
                return false;
            }

            const auto owner = owner_of(id);

            return
                owner != invalid_id &&
                v.syntax_.declarations[
                    declaration_ids.at(owner)
                ].kind == DeclarationKind::model;
        };

        if (declaration.kind == DeclarationKind::event) {
            if (
                !declaration.methods.empty() ||
                !declaration.metadata.empty() ||
                std::any_of(
                    declaration.fields.begin(),
                    declaration.fields.end(),
                    [](const auto& field) {
                        return field.injection;
                    }
                )
            ) {
                report(
                    declaration.origin,
                    "Events contain immutable data fields only",
                    "GNR2305"
                );
            }
        }

        if (declaration.kind == DeclarationKind::middleware) {
            const auto index = method_index("handle");

            if (index == invalid_id) {
                report(
                    declaration.origin,
                    "Middleware requires handle(Request request, Next next)",
                    "GNR2301"
                );
            } else {
                const auto& method = declaration.methods[index];
                const auto& function =
                    v.symbols_[resolved.methods[index].symbol];

                const bool parameters_valid =
                    function.parameters.size() == 2 &&
                    function.parameters[0] == type_id("Request") &&
                    function.parameters[1] == type_id("Next");

                const bool defaults_valid =
                    std::none_of(
                        method.parameters.begin(),
                        method.parameters.end(),
                        [](const auto& parameter) {
                            return parameter.default_value != invalid_id;
                        }
                    );

                if (
                    method.visibility != Visibility::public_ ||
                    !parameters_valid ||
                    !defaults_valid ||
                    function.type != type_id("Response")
                ) {
                    report(
                        method.origin,
                        "Middleware handle must be public and have the contract Response handle(Request, Next); async handle uses the same logical Response result",
                        "GNR2301"
                    );
                }
            }
        }

        if (declaration.kind == DeclarationKind::migration) {
            for (const auto& name : {"up", "down"}) {
                const auto index = method_index(name);

                if (index == invalid_id) {
                    report(
                        declaration.origin,
                        "Migration requires public synchronous void up() and down() methods",
                        "GNR2302"
                    );
                    continue;
                }

                const auto& method = declaration.methods[index];
                const auto& function =
                    v.symbols_[resolved.methods[index].symbol];

                if (
                    method.visibility != Visibility::public_ ||
                    method.asynchronous ||
                    !method.parameters.empty() ||
                    function.type != type_id("void")
                ) {
                    report(
                        method.origin,
                        "Migration requires public synchronous void up() and down() methods",
                        "GNR2302"
                    );
                }
            }
        }

        if (
            declaration.kind == DeclarationKind::listener ||
            declaration.kind == DeclarationKind::job
        ) {
            const auto index = method_index("handle");

            if (index == invalid_id) {
                report(
                    declaration.origin,
                    declaration.kind == DeclarationKind::listener
                        ? "Listener requires handle(Event event)"
                        : "Job requires handle()",
                    "GNR2303"
                );
            } else {
                const auto& method = declaration.methods[index];
                const auto& function =
                    v.symbols_[resolved.methods[index].symbol];

                if (
                    method.visibility != Visibility::public_ ||
                    function.type != type_id("void")
                ) {
                    report(
                        method.origin,
                        "Listener and job handle methods must be public and return void",
                        "GNR2303"
                    );
                }

                if (declaration.kind == DeclarationKind::listener) {
                    const bool one_event =
                        function.parameters.size() == 1 &&
                        !type(function.parameters[0]).optional;

                    bool event_type = false;

                    if (one_event) {
                        const auto owner =
                            owner_of(function.parameters[0]);

                        event_type =
                            owner != invalid_id &&
                            v.syntax_.declarations[
                                declaration_ids.at(owner)
                            ].kind == DeclarationKind::event;
                    }

                    if (
                        !one_event ||
                        !event_type ||
                        (!method.parameters.empty() &&
                         method.parameters[0].default_value != invalid_id)
                    ) {
                        report(
                            method.origin,
                            "Listener handle requires one non-optional event parameter without a default value",
                            "GNR2303"
                        );
                    }
                } else if (!method.parameters.empty()) {
                    report(
                        method.origin,
                        "Job handle() does not accept parameters",
                        "GNR2303"
                    );
                }
            }
        }

        if (declaration.kind == DeclarationKind::policy) {
            std::size_t abilities = 0;

            for (
                std::size_t m = 0;
                m < declaration.methods.size();
                ++m
            ) {
                const auto& method = declaration.methods[m];

                if (method.visibility != Visibility::public_) {
                    continue;
                }

                ++abilities;

                const auto& function =
                    v.symbols_[resolved.methods[m].symbol];

                const bool defaults_valid =
                    std::none_of(
                        method.parameters.begin(),
                        method.parameters.end(),
                        [](const auto& parameter) {
                            return parameter.default_value != invalid_id;
                        }
                    );

                if (
                    method.asynchronous ||
                    function.parameters.size() != 2 ||
                    !defaults_valid ||
                    function.type != type_id("Decision") ||
                    !is_model_type(
                        function.parameters.size() > 0
                            ? function.parameters[0]
                            : invalid_id
                    ) ||
                    !is_model_type(
                        function.parameters.size() > 1
                            ? function.parameters[1]
                            : invalid_id
                    )
                ) {
                    report(
                        method.origin,
                        "Public policy abilities must be synchronous Decision methods with exactly two non-optional model parameters: actor and resource",
                        "GNR2304"
                    );
                }
            }

            if (abilities == 0) {
                report(
                    declaration.origin,
                    "Policy requires at least one public actor/resource ability",
                    "GNR2304"
                );
            }
        }

        if (declaration.kind == DeclarationKind::notification) {
            const auto via_index = method_index("via");

            if (via_index == invalid_id) {
                report(
                    declaration.origin,
                    "Notification requires public synchronous via(Recipient recipient)",
                    "GNR2306"
                );
            } else {
                const auto& via = declaration.methods[via_index];
                const auto& via_function =
                    v.symbols_[resolved.methods[via_index].symbol];

                const bool recipient_valid =
                    via_function.parameters.size() == 1 &&
                    is_model_type(via_function.parameters[0]);

                const bool default_free =
                    via.parameters.size() == 1 &&
                    via.parameters[0].default_value == invalid_id;

                if (
                    via.visibility != Visibility::public_ ||
                    via.asynchronous ||
                    !recipient_valid ||
                    !default_free ||
                    via_function.type !=
                        sequence("List", type_id("string"))
                ) {
                    report(
                        via.origin,
                        "Notification via must be public, synchronous, accept one non-optional model recipient, and return List<string>",
                        "GNR2306"
                    );
                }

                for (
                    std::size_t m = 0;
                    m < declaration.methods.size();
                    ++m
                ) {
                    const auto& method = declaration.methods[m];

                    if (
                        method.name != "toMail" &&
                        method.name != "toDatabase"
                    ) {
                        continue;
                    }

                    const auto& function =
                        v.symbols_[resolved.methods[m].symbol];

                    if (
                        method.visibility != Visibility::public_ ||
                        method.asynchronous ||
                        function.parameters != via_function.parameters ||
                        method.parameters.size() != 1 ||
                        method.parameters[0].default_value != invalid_id
                    ) {
                        report(
                            method.origin,
                            "Notification payload methods must use the same public synchronous recipient contract as via()",
                            "GNR2306"
                        );
                        continue;
                    }

                    if (method.name == "toMail") {
                        const auto owner = owner_of(function.type);

                        if (
                            owner == invalid_id ||
                            v.syntax_.declarations[
                                declaration_ids.at(owner)
                            ].kind != DeclarationKind::mail
                        ) {
                            report(
                                method.origin,
                                "Notification toMail must return a structured mail declaration",
                                "GNR2306"
                            );
                        }
                    } else if (
                        type(function.type).name != "Json" &&
                        type(function.type).name != "Data"
                    ) {
                        report(
                            method.origin,
                            "Notification toDatabase must return Json",
                            "GNR2306"
                        );
                    }
                }
            }
        }

        if (declaration.kind == DeclarationKind::mail) {
            const auto subject_index = method_index("subject");

            if (subject_index == invalid_id) {
                report(
                    declaration.origin,
                    "Mail requires subject()",
                    "GNR2307"
                );
            }

            bool has_html = false;
            bool has_content = false;

            for (
                std::size_t m = 0;
                m < declaration.methods.size();
                ++m
            ) {
                const auto& method = declaration.methods[m];

                if (
                    method.name != "subject" &&
                    method.name != "text" &&
                    method.name != "html" &&
                    method.name != "content"
                ) {
                    continue;
                }

                const auto& function =
                    v.symbols_[resolved.methods[m].symbol];

                if (
                    method.asynchronous ||
                    !method.parameters.empty()
                ) {
                    report(
                        method.origin,
                        "Mail composition methods must be synchronous and parameterless",
                        "GNR2307"
                    );
                }

                if (
                    (method.name == "subject" ||
                     method.name == "text" ||
                     method.name == "html") &&
                    function.type != type_id("string")
                ) {
                    report(
                        method.origin,
                        "Mail subject/text/html methods must return string",
                        "GNR2307"
                    );
                }

                if (
                    method.name == "content" &&
                    function.type != type_id("Response")
                ) {
                    report(
                        method.origin,
                        "Mail content() must return Response",
                        "GNR2307"
                    );
                }

                has_html |= method.name == "html";
                has_content |= method.name == "content";
            }

            if (has_html && has_content) {
                report(
                    declaration.origin,
                    "Mail cannot declare both html() and content(); choose one HTML body source",
                    "GNR2307"
                );
            }
        }

        std::unordered_set<std::string> metadata_names;
        static const std::unordered_set<std::string>
            metadata_allowed{
                "table",
                "connection",
                "fillable",
                "hidden",
                "visible",
                "casts",
                "timestamps",
                "softDeletes",
                "primaryKey",
                "incrementing"
            };

        for (const auto& metadata : declaration.metadata) {
            if (
                declaration.kind != DeclarationKind::model ||
                !metadata_allowed.contains(metadata.name)
            ) {
                report(
                    metadata.origin,
                    "Unknown framework metadata '" +
                        metadata.name + "'",
                    "GNR2308"
                );
                continue;
            }

            if (!metadata_names.insert(metadata.name).second) {
                report(
                    metadata.origin,
                    "Duplicate metadata '" +
                        metadata.name + "'",
                    "GNR2308"
                );
            }

            const auto& expr =
                v.syntax_.expressions[metadata.value];

            if (
                metadata.name == "fillable" ||
                metadata.name == "hidden" ||
                metadata.name == "visible"
            ) {
                if (expr.kind != SyntaxExpressionKind::list) {
                    report(
                        metadata.origin,
                        "Attribute metadata requires a constant string list",
                        "GNR2308"
                    );
                }

                for (auto id : expr.operands) {
                    if (
                        v.syntax_.expressions[id].literal_type !=
                        "string"
                    ) {
                        report(
                            metadata.origin,
                            "Attribute names must be string constants",
                            "GNR2308"
                        );
                    }
                }
            } else if (metadata.name == "casts") {
                if (expr.kind != SyntaxExpressionKind::object) {
                    report(
                        metadata.origin,
                        "casts requires a constant object",
                        "GNR2308"
                    );
                }

                static const std::unordered_set<std::string>
                    supported{
                        "bool",
                        "int",
                        "integer",
                        "string",
                        "double",
                        "decimal",
                        "json",
                        "date",
                        "datetime"
                    };

                for (auto id : expr.operands) {
                    if (
                        v.syntax_.expressions[id].literal_type !=
                            "string" ||
                        !supported.contains(
                            v.syntax_.expressions[id].text
                        )
                    ) {
                        report(
                            metadata.origin,
                            "Unknown model cast",
                            "GNR2308"
                        );
                    }
                }
            } else if (
                metadata.name == "timestamps" ||
                metadata.name == "softDeletes" ||
                metadata.name == "incrementing"
            ) {
                if (expr.literal_type != "bool") {
                    report(
                        metadata.origin,
                        "Metadata requires a boolean constant",
                        "GNR2308"
                    );
                }
            } else if (expr.literal_type != "string") {
                report(
                    metadata.origin,
                    "Metadata requires a string constant",
                    "GNR2308"
                );
            }

            if (
                (metadata.name == "table" ||
                 metadata.name == "connection") &&
                expr.literal_type == "string" &&
                expr.text.empty()
            ) {
                report(
                    metadata.origin,
                    metadata.name +
                        " metadata cannot be empty",
                    "GNR2308"
                );
            }

            if (
                metadata.name == "primaryKey" &&
                expr.literal_type == "string" &&
                !valid_identifier(expr.text)
            ) {
                report(
                    metadata.origin,
                    "primaryKey must name a valid model attribute",
                    "GNR2308"
                );
            }

            expression(metadata.value);
        }

        if (declaration.kind == DeclarationKind::model) {
            std::string primary = "id";
            bool incrementing = true;
            bool incrementing_explicit = false;
            bool timestamps = false;
            bool soft_deletes = false;

            for (const auto& metadata : declaration.metadata) {
                const auto& expr =
                    v.syntax_.expressions[metadata.value];

                if (
                    metadata.name == "primaryKey" &&
                    expr.literal_type == "string"
                ) {
                    primary = expr.text;
                }

                if (
                    metadata.name == "incrementing" &&
                    expr.literal_type == "bool"
                ) {
                    incrementing_explicit = true;
                    incrementing =
                        expr.text == "true";
                }

                if (
                    metadata.name == "timestamps" &&
                    expr.literal_type == "bool"
                ) {
                    timestamps =
                        expr.text == "true";
                }

                if (
                    metadata.name == "softDeletes" &&
                    expr.literal_type == "bool"
                ) {
                    soft_deletes =
                        expr.text == "true";
                }
            }

            const auto field_type = [&](std::string_view name) {
                for (
                    std::size_t i = 0;
                    i < declaration.fields.size() &&
                    i < resolved.fields.size();
                    ++i
                ) {
                    if (declaration.fields[i].name == name) {
                        return v.symbols_[
                            resolved.fields[i]
                        ].type;
                    }
                }

                return invalid_id;
            };

            const auto primary_type =
                field_type(primary);

            if (
                primary_type == invalid_id ||
                type(primary_type).optional ||
                (
                    type(primary_type).name != "int" &&
                    type(primary_type).name != "uint64" &&
                    type(primary_type).name != "string"
                )
            ) {
                report(
                    declaration.origin,
                    "Model primary key must be a non-optional int, uint64, or string attribute",
                    "GNR2308"
                );
            }

            if (
                incrementing_explicit &&
                incrementing &&
                primary_type != invalid_id &&
                type(primary_type).name != "int" &&
                type(primary_type).name != "uint64"
            ) {
                report(
                    declaration.origin,
                    "incrementing=true requires an integer primary key",
                    "GNR2308"
                );
            }

            const auto lifecycle_string =
                [&](std::string_view name) {
                    const auto id = field_type(name);

                    return
                        id != invalid_id &&
                        type(id).name == "string" &&
                        type(id).optional;
                };

            if (
                timestamps &&
                (
                    !lifecycle_string("created_at") ||
                    !lifecycle_string("updated_at")
                )
            ) {
                report(
                    declaration.origin,
                    "timestamps=true requires nullable string created_at and updated_at attributes",
                    "GNR2308"
                );
            }

            if (
                soft_deletes &&
                !lifecycle_string("deleted_at")
            ) {
                report(
                    declaration.origin,
                    "softDeletes=true requires a nullable string deleted_at attribute",
                    "GNR2308"
                );
            }
        }
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
            statements(method.body); resolved.all_paths_return = block_flow(method.body).terminates_path();
            if (return_type == type_id("inferred")) {
                auto inferred = returned.empty() ? type_id("void") : returned.front();
                for (std::size_t i = 1; i < returned.size(); ++i) {
                    const auto common = common_type(inferred, returned[i]);
                    if (!common) {
                        report(method.origin,"Inferred returns have incompatible types");
                        inferred = type_id("Value");
                        break;
                    }
                    inferred = *common;
                }
                v.symbols_[resolved.symbol].type = inferred;
                return_type = inferred;
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
