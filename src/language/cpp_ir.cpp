#include <gungnir/language/cpp_ir.hpp>
#include <gungnir/language/compiler.hpp>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace gungnir::language {
namespace {
std::string quote(std::string_view value) { std::ostringstream out; out << '"'; for (unsigned char c : value) { if (c == '\\' || c == '"') out << '\\' << c; else if (c == '\n') out << "\\n"; else if (c == '\r') out << "\\r"; else if (c == '\t') out << "\\t"; else if (c < 32) out << '\\' << std::oct << std::setw(3) << std::setfill('0') << unsigned(c) << std::dec; else out << c; } out << '"'; return out.str(); }
class CppIrLoweringRenderer {
    const ValidatedProject& p;
    const SyntaxProject& s;
    bool lines, async = false;
    TypeId return_type = invalid_id;
    std::ostringstream out;
    std::unordered_set<SymbolId> captured;
    std::unordered_set<SymbolId> model_fields;
    std::string type(TypeId id) const { return p.types().at(id).cpp_name; }
    const ResolvedSymbol& symbol(SymbolId id) const { return p.symbols().at(id); }
    std::string access(SymbolId id, bool raw = false) const { const auto& v = symbol(id); std::string name = captured.contains(id) ? "gnr_capture_" + std::to_string(id) : v.cpp_name; if (!raw && !captured.contains(id) && model_fields.contains(id)) name += ".get()"; return name; }
    std::string convert(std::string value, TypeId target, TypeId source) const {
        if (target == source) return value;
        if (p.types()[source].name == "null") return p.types()[target].optional ? "std::nullopt" : "gungnir::Json{nullptr}";
        const auto& t = p.types()[target];
        if (t.name == "Json" || t.name == "Data" || t.name == "Value") return "gungnir::http::make_json(" + value + ")";
        if (t.name == "Decision" && p.types()[source].name == "bool") return "gungnir::auth::Decision{" + value + ", {}}";
        return type(target) + "(" + value + ")";
    }
    std::string expr(SyntaxId id, bool raw = false) {
        if (id == invalid_id) return {};
        const auto& e = s.expressions.at(id); const auto& r = p.expressions().at(id);
        auto child = [&](std::size_t i) { return expr(e.operands.at(i)); };
        switch (e.kind) {
        case SyntaxExpressionKind::literal:
            if (e.literal_type == "string") return "gungnir::String{" + quote(e.text) + ", " + std::to_string(e.text.size()) + "}";
            if (e.literal_type == "null") return "nullptr";
            return type(r.type) + "(" + e.text + ")";
        case SyntaxExpressionKind::name: { auto value = access(r.symbol,raw); if (p.types()[symbol(r.symbol).type].optional && !p.types()[r.type].optional) value += ".value()"; return value; }
        case SyntaxExpressionKind::member: {
            if (e.literal_type == "::" && r.symbol != invalid_id && symbol(r.symbol).cpp_name.starts_with("::")) return access(r.symbol,raw);
            const auto& receiver = p.expressions()[e.operands[0]];
            const bool injection = receiver.symbol != invalid_id && symbol(receiver.symbol).kind == ResolvedSymbolKind::injection;
            auto value = child(0) + (injection ? "->" : e.literal_type == "::" ? "::" : ".") + symbol(r.symbol).cpp_name;
            if (!raw && model_fields.contains(r.symbol)) value += ".get()";
            if (e.literal_type == "?.") { const auto base = child(0); value = "([&]() -> " + type(r.type) + " { auto gnr_optional = " + base + "; if (!gnr_optional) return std::nullopt; return gnr_optional->" + symbol(r.symbol).cpp_name + (model_fields.contains(r.symbol) ? ".get()" : "") + "; }())"; }
            return value;
        }
        case SyntaxExpressionKind::await_: return "(co_await " + child(0) + ")";
        case SyntaxExpressionKind::unary: return e.text.starts_with("post") ? "(" + expr(e.operands[0],true) + e.text.substr(4) + ")" : "(" + e.text + child(0) + ")";
        case SyntaxExpressionKind::binary:
            if (e.text == "==" || e.text == "!=") { const auto lhs = s.expressions[e.operands[0]].literal_type == "null" ? "std::nullopt" : child(0); const auto rhs = s.expressions[e.operands[1]].literal_type == "null" ? "std::nullopt" : child(1); return "(" + std::string(lhs) + " " + e.text + " " + rhs + ")"; }
            if (e.text == "=" || e.text == "+=" || e.text == "-=" || e.text == "*=" || e.text == "/=") return "(" + expr(e.operands[0],true) + " " + e.text + " " + convert(child(1),r.type,p.expressions()[e.operands[1]].type) + ")";
            if (e.text == "??") return "([&]() -> " + type(r.type) + " { auto gnr_optional = " + child(0) + "; if (gnr_optional) return *gnr_optional; return " + child(1) + "; }())";
            return "(" + expr(e.operands[0],e.text == "=" || e.text.ends_with("=" ) && e.text.size() == 2 && e.text != "==" && e.text != "!=" && e.text != "<=" && e.text != ">=") + " " + e.text + " " + child(1) + ")";
        case SyntaxExpressionKind::conditional: return "(" + child(0) + " ? " + convert(child(1),r.type,p.expressions()[e.operands[1]].type) + " : " + convert(child(2),r.type,p.expressions()[e.operands[2]].type) + ")";
        case SyntaxExpressionKind::subscript: { const auto name = p.types()[p.expressions()[e.operands[0]].type].name; return name == "Json" || name == "Data" ? "gungnir::language::runtime::lookup(" + child(0) + "," + child(1) + ")" : child(0) + ".at(" + child(1) + ")"; }
        case SyntaxExpressionKind::list: {
            std::string result = type(r.type) + "{"; for (std::size_t i = 0; i < e.operands.size(); ++i) { if (i) result += ","; result += convert(child(i),p.types()[r.type].arguments[0],p.expressions()[e.operands[i]].type); } return result + "}";
        }
        case SyntaxExpressionKind::object: {
            std::string result = "gungnir::Json::object({"; for (std::size_t i = 0; i < e.operands.size(); ++i) { if (i) result += ","; result += "{" + quote(e.argument_names[i]) + ",gungnir::http::make_json(" + child(i) + ")}"; } return result + "})";
        }
        case SyntaxExpressionKind::lambda: {
            const auto saved = captured; std::string result = "[";
            for (std::size_t i = 0; i < r.captures.size(); ++i) { if (i) result += ','; auto cap = r.captures[i]; result += "gnr_capture_" + std::to_string(cap) + " = " + access(cap); }
            result += "](";
            for (std::size_t i = 0; i < r.parameters.size(); ++i) { if (i) result += ','; const auto& parameter = symbol(r.parameters[i]); result += type(parameter.type) + (p.types()[parameter.type].name == "Column" ? "& " : " ") + parameter.cpp_name; }
            result += ") -> " + type(p.types()[r.type].arguments.back()) + " {\n";
            for (auto cap : r.captures) captured.insert(cap);
            const auto previous_async = async; const auto previous_return = return_type; return_type = p.types()[r.type].arguments.back(); async = false; for (auto statement : e.body) result += stmt(statement); async = previous_async; return_type = previous_return; captured = saved; return result + "}";
        }
        case SyntaxExpressionKind::call: {
            const auto& callable = symbol(r.symbol); const auto& callee = s.expressions[e.operands[0]];
            std::string target = callable.cpp_name, receiver;
            if (callable.owner != invalid_id && p.types()[symbol(callable.owner).type].name == "Next")
                return access(callable.owner) + "(" + expr(e.operands.at(1)) + ")";
            if (callable.owner != invalid_id && (symbol(callable.owner).kind == ResolvedSymbolKind::local || symbol(callable.owner).kind == ResolvedSymbolKind::parameter)) target = access(callable.owner);
            bool receiver_argument = false;
            if (callee.kind == SyntaxExpressionKind::member) {
                const auto rr = p.expressions()[callee.operands[0]]; const auto& sr = s.expressions[callee.operands[0]];
                const bool is_static = sr.kind == SyntaxExpressionKind::name && rr.symbol != invalid_id && (symbol(rr.symbol).kind == ResolvedSymbolKind::declaration || symbol(rr.symbol).kind == ResolvedSymbolKind::builtin);
                const bool injection = rr.symbol != invalid_id && symbol(rr.symbol).kind == ResolvedSymbolKind::injection;
                if (!target.starts_with("gungnir::") && !target.starts_with("::")) {
                    if (is_static) target = expr(callee.operands[0]) + "::" + target;
                    else { receiver = expr(callee.operands[0]); target = "gungnir::language::runtime::receiver(std::get<0>(gnr_values))" + std::string(injection ? "->" : ".") + target; }
                } else if (target.starts_with("gungnir::language::runtime::") && (callable.name == "map" || callable.name == "filter" || callable.name == "each" || callable.name == "validate")) { receiver = expr(callee.operands[0]); receiver_argument = true; }
            }
            // Braced tuple construction fixes evaluation order, including await
            // expressions, before named arguments are reordered for the call.
            std::string tuple = "std::tuple{";
            if (!receiver.empty()) tuple += "gungnir::language::runtime::hold_receiver(" + receiver + ")";
            for (std::size_t i = 1; i < e.operands.size(); ++i) { if (tuple.back() != '{') tuple += ','; tuple += child(i); }
            tuple += '}';
            std::string arguments = receiver_argument ? "gungnir::language::runtime::receiver(std::get<0>(gnr_values))" : "";
            for (std::size_t i = 0; i < r.argument_order.size(); ++i) {
                const auto index = r.argument_order[i];
                if (index == invalid_id && callable.defaults[i] == invalid_id-1) continue;
                if (!arguments.empty()) arguments += ',';
                if (index == invalid_id) arguments += expr(callable.defaults[i]);
                else {
                    auto value = convert("std::get<" + std::to_string(index + (receiver.empty() ? 0 : 1)) + ">(gnr_values)",r.argument_conversions[i],p.expressions()[e.operands[index+1]].type);
                    if ((callable.name == "create" || callable.name == "update") && callee.kind == SyntaxExpressionKind::member && p.types()[p.expressions()[e.operands[index+1]].type].name == "Json") value = "gungnir::language::runtime::attributes(" + value + ")";
                    arguments += value;
                }
            }
            auto invocation = target + "(" + arguments + ")";
            if (!callable.asynchronous && p.types()[r.type].name == "string") invocation = "gungnir::String(" + invocation + ")";
            return "([&](auto&& gnr_values) -> decltype(auto) { return " + invocation + "; }(" + tuple + "))";
        }
        }
        return {};
    }
    std::string stmt(SyntaxId id) {
        const auto& node = s.statements[id]; auto block = [&](const auto& body) { std::string result = "{\n"; for (auto id : body) result += stmt(id); return result + "}\n"; };
        switch (node.kind) {
        case SyntaxStatementKind::binding: { const auto& b = symbol(p.bindings()[id]); return (b.immutable ? "const " : "") + (p.types()[b.type].name == "Function" ? "auto" : type(b.type)) + " " + b.cpp_name + " = " + convert(expr(node.expression),b.type,p.expressions()[node.expression].type) + ";\n"; }
        case SyntaxStatementKind::expression: return expr(node.expression) + ";\n";
        case SyntaxStatementKind::return_: return std::string(async ? "co_return" : "return") + (node.expression == invalid_id ? "" : " " + (return_type == invalid_id ? expr(node.expression) : convert(expr(node.expression),return_type,p.expressions()[node.expression].type))) + ";\n";
        case SyntaxStatementKind::throw_: return "throw " + expr(node.expression) + ";\n";
        case SyntaxStatementKind::block: return block(node.body);
        case SyntaxStatementKind::if_: return "if (" + expr(node.expression) + ") " + block(node.body) + (node.alternative.empty() ? "" : "else " + block(node.alternative));
        case SyntaxStatementKind::while_: return "while (" + expr(node.expression) + ") " + block(node.body);
        case SyntaxStatementKind::for_in: return "for (const auto& " + node.name + " : " + expr(node.expression) + ") " + block(node.body);
        case SyntaxStatementKind::for_: { std::string init; for (auto id : node.parts) init += stmt(id); std::string step; for (auto id : node.alternative) { if (!step.empty()) step += ','; step += expr(s.statements[id].expression); } return "{\n" + init + "for (;" + expr(node.expression) + ";" + step + ") " + block(node.body) + "}\n"; }
        case SyntaxStatementKind::break_: return "break;\n";
        case SyntaxStatementKind::continue_: return "continue;\n";
        }
        return {};
    }

    static CppIrExpressionKind ir_expression_kind(
        SyntaxExpressionKind kind
    ) {
        switch (kind) {
        case SyntaxExpressionKind::literal:
            return CppIrExpressionKind::literal;
        case SyntaxExpressionKind::name:
            return CppIrExpressionKind::name;
        case SyntaxExpressionKind::member:
            return CppIrExpressionKind::member;
        case SyntaxExpressionKind::call:
            return CppIrExpressionKind::call;
        case SyntaxExpressionKind::unary:
            return CppIrExpressionKind::unary;
        case SyntaxExpressionKind::binary:
            return CppIrExpressionKind::binary;
        case SyntaxExpressionKind::subscript:
            return CppIrExpressionKind::subscript;
        case SyntaxExpressionKind::list:
            return CppIrExpressionKind::list;
        case SyntaxExpressionKind::object:
            return CppIrExpressionKind::object;
        case SyntaxExpressionKind::lambda:
            return CppIrExpressionKind::lambda;
        case SyntaxExpressionKind::await_:
            return CppIrExpressionKind::await_;
        case SyntaxExpressionKind::conditional:
            return CppIrExpressionKind::conditional;
        }
        throw std::logic_error("Unknown syntax expression kind");
    }

    static CppIrSource ir_source(const Origin& origin) {
        return CppIrSource{
            origin.file,
            origin.line,
            origin.column
        };
    }

    CppIrId lower_expression(
        CppIrProject& ir,
        SyntaxId id,
        bool raw = false
    ) {
        if (id == invalid_id) {
            return invalid_cpp_ir_id;
        }

        const auto& syntax = s.expressions.at(id);
        const auto& resolution = p.expressions().at(id);

        CppIrExpression node;
        node.kind = ir_expression_kind(syntax.kind);
        node.type = CppIrType{type(resolution.type)};
        node.spelling = expr(id, raw);

        const auto result_id = ir.expressions.size();
        ir.expressions.push_back(std::move(node));

        for (auto operand : syntax.operands) {
            ir.expressions[result_id].operands.push_back(
                lower_expression(ir, operand)
            );
        }

        if (syntax.kind == SyntaxExpressionKind::lambda) {
            const auto saved_captured = captured;
            const auto saved_async = async;
            const auto saved_return = return_type;

            for (auto capture : resolution.captures) {
                captured.insert(capture);
            }

            async = false;
            return_type =
                p.types()[resolution.type].arguments.back();

            for (auto statement : syntax.body) {
                ir.expressions[result_id].body.push_back(
                    lower_statement(ir, statement)
                );
            }

            captured = saved_captured;
            async = saved_async;
            return_type = saved_return;
        }

        return result_id;
    }

    CppIrId lower_conversion(
        CppIrProject& ir,
        SyntaxId id,
        TypeId target
    ) {
        if (id == invalid_id) {
            return invalid_cpp_ir_id;
        }

        const auto source = p.expressions().at(id).type;
        const auto value = lower_expression(ir, id);

        if (target == invalid_id || target == source) {
            return value;
        }

        CppIrExpression converted;
        converted.kind = CppIrExpressionKind::conversion;
        converted.type = CppIrType{type(target)};
        converted.spelling = convert(
            ir.expressions[value].spelling,
            target,
            source
        );
        converted.operands.push_back(value);

        const auto converted_id = ir.expressions.size();
        ir.expressions.push_back(std::move(converted));
        return converted_id;
    }

    CppIrId lower_statement(
        CppIrProject& ir,
        SyntaxId id
    ) {
        const auto& syntax = s.statements.at(id);

        CppIrStatement node;
        node.source = ir_source(syntax.origin);

        switch (syntax.kind) {
        case SyntaxStatementKind::binding: {
            const auto& binding = symbol(p.bindings().at(id));
            node.kind = CppIrStatementKind::binding;
            node.name = binding.cpp_name;
            node.type = CppIrType{
                p.types()[binding.type].name == "Function"
                    ? "auto"
                    : type(binding.type)
            };
            node.immutable = binding.immutable;
            node.expression = lower_conversion(
                ir,
                syntax.expression,
                binding.type
            );
            break;
        }
        case SyntaxStatementKind::expression:
            node.kind = CppIrStatementKind::expression;
            node.expression = lower_expression(
                ir,
                syntax.expression
            );
            break;
        case SyntaxStatementKind::return_:
            node.kind = async
                ? CppIrStatementKind::co_return
                : CppIrStatementKind::return_;
            node.expression = return_type == invalid_id
                ? lower_expression(ir, syntax.expression)
                : lower_conversion(
                    ir,
                    syntax.expression,
                    return_type
                );
            break;
        case SyntaxStatementKind::throw_:
            node.kind = CppIrStatementKind::throw_;
            node.expression = lower_expression(
                ir,
                syntax.expression
            );
            break;
        case SyntaxStatementKind::block:
            node.kind = CppIrStatementKind::block;
            for (auto child : syntax.body) {
                node.body.push_back(
                    lower_statement(ir, child)
                );
            }
            break;
        case SyntaxStatementKind::if_:
            node.kind = CppIrStatementKind::if_;
            node.expression = lower_expression(
                ir,
                syntax.expression
            );
            for (auto child : syntax.body) {
                node.body.push_back(
                    lower_statement(ir, child)
                );
            }
            for (auto child : syntax.alternative) {
                node.alternative.push_back(
                    lower_statement(ir, child)
                );
            }
            break;
        case SyntaxStatementKind::while_:
            node.kind = CppIrStatementKind::while_;
            node.expression = lower_expression(
                ir,
                syntax.expression
            );
            for (auto child : syntax.body) {
                node.body.push_back(
                    lower_statement(ir, child)
                );
            }
            break;
        case SyntaxStatementKind::for_in:
            node.kind = CppIrStatementKind::for_in;
            node.name = syntax.name;
            node.expression = lower_expression(
                ir,
                syntax.expression
            );
            for (auto child : syntax.body) {
                node.body.push_back(
                    lower_statement(ir, child)
                );
            }
            break;
        case SyntaxStatementKind::for_:
            node.kind = CppIrStatementKind::for_;
            node.expression = lower_expression(
                ir,
                syntax.expression
            );
            for (auto child : syntax.parts) {
                node.parts.push_back(
                    lower_statement(ir, child)
                );
            }
            for (auto child : syntax.alternative) {
                node.alternative.push_back(
                    lower_statement(ir, child)
                );
            }
            for (auto child : syntax.body) {
                node.body.push_back(
                    lower_statement(ir, child)
                );
            }
            break;
        case SyntaxStatementKind::break_:
            node.kind = CppIrStatementKind::break_;
            break;
        case SyntaxStatementKind::continue_:
            node.kind = CppIrStatementKind::continue_;
            break;
        }

        const auto result_id = ir.statements.size();
        ir.statements.push_back(std::move(node));
        return result_id;
    }

    void lower_functions(CppIrProject& ir) {
        std::vector<std::size_t> unit_for_module(
            s.modules.size(),
            invalid_id
        );

        for (auto module : p.module_order()) {
            unit_for_module[module] = ir.units.size();
            ir.units.push_back(CppIrUnit{
                s.modules[module].name,
                {}
            });
        }

        for (
            std::size_t declaration_id = 0;
            declaration_id < s.declarations.size();
            ++declaration_id
        ) {
            const auto& declaration =
                s.declarations[declaration_id];
            const auto& declaration_resolution =
                p.declarations()[declaration_id];

            for (
                std::size_t method_id = 0;
                method_id < declaration.methods.size();
                ++method_id
            ) {
                const auto& method =
                    declaration.methods[method_id];
                const auto& method_resolution =
                    declaration_resolution.methods[method_id];
                const auto& callable =
                    symbol(method_resolution.symbol);

                CppIrFunction function;
                function.module =
                    s.modules[declaration.module].name;
                function.owner =
                    declaration.kind == DeclarationKind::function
                        ? std::string{}
                        : declaration.name;
                function.name = method.name;
                function.result = CppIrType{
                    result(method, method_resolution)
                };
                function.coroutine = method.asynchronous;
                function.line_directive = lines;
                function.source = ir_source(method.origin);

                for (
                    std::size_t parameter_id = 0;
                    parameter_id < method.parameters.size();
                    ++parameter_id
                ) {
                    const auto parameter_type =
                        callable.parameters[parameter_id];
                    function.parameters.push_back(
                        CppIrParameter{
                            CppIrType{type(parameter_type)},
                            method.parameters[parameter_id].name,
                            p.types()[parameter_type].name ==
                                "Request",
                            invalid_cpp_ir_id
                        }
                    );
                }

                const auto saved_async = async;
                const auto saved_return = return_type;
                const auto saved_captured = captured;

                async = method.asynchronous;
                return_type = callable.type;
                captured.clear();

                for (auto statement : method.body) {
                    function.body.push_back(
                        lower_statement(ir, statement)
                    );
                }

                if (
                    method.asynchronous &&
                    type(callable.type) == "void"
                ) {
                    CppIrStatement terminal;
                    terminal.kind =
                        CppIrStatementKind::co_return;
                    terminal.source =
                        ir_source(method.origin);
                    const auto terminal_id =
                        ir.statements.size();
                    ir.statements.push_back(
                        std::move(terminal)
                    );
                    function.body.push_back(terminal_id);
                }

                async = saved_async;
                return_type = saved_return;
                captured = saved_captured;

                const auto function_id =
                    ir.functions.size();
                ir.functions.push_back(
                    std::move(function)
                );

                const auto unit =
                    unit_for_module[
                        declaration.module
                    ];
                if (unit != invalid_id) {
                    ir.units[unit].functions.push_back(
                        function_id
                    );
                }
            }
        }
    }

    std::string result(const CallableSyntax& method, const CallableResolution& resolved) const { auto value = type(symbol(resolved.symbol).type); return method.asynchronous ? "gungnir::Task<" + value + ">" : value; }
    std::string params(const CallableSyntax& method,const CallableResolution& resolved,bool defaults = false) {
        std::string result; const auto& fn = symbol(resolved.symbol);
        for (std::size_t i = 0; i < method.parameters.size(); ++i) { if (i) result += ','; result += type(fn.parameters[i]) + (p.types()[fn.parameters[i]].name == "Request" ? "& " : " ") + method.parameters[i].name; if (defaults && method.parameters[i].default_value != invalid_id) result += " = " + expr(method.parameters[i].default_value); } return result;
    }
    std::string ns(std::size_t module) const { std::string name = s.modules[module].name; std::string value = "gnr"; for (char c : name) value += c == '.' ? "::" : std::string{c}; return name.empty() ? "" : "gnr::" + value.substr(3); }
    void open(std::size_t module) { if (!ns(module).empty()) out << "namespace " << ns(module) << " {\n"; }
    void close(std::size_t module) { if (!ns(module).empty()) out << "}\n"; }
    void origin(const Origin& origin) { if (lines) out << "#line " << origin.line << ' ' << quote(origin.file) << '\n'; }
public:
    CppIrLoweringRenderer(const ValidatedProject& project,bool lines) : p(project),s(project.syntax()),lines(lines) { for (std::size_t d = 0; d < s.declarations.size(); ++d) if (s.declarations[d].kind == DeclarationKind::model) for (auto f : p.declarations()[d].fields) model_fields.insert(f); }
    void lower_structural_functions(CppIrProject& ir) {
        lower_functions(ir);
    }
    std::string declarations() {
        out << "// Generated from a validated Gungnir program.\n#include <gungnir/language/runtime.hpp>\n#include <optional>\n#include <tuple>\n";
        for (std::size_t d = 0; d < s.declarations.size(); ++d) { const auto& decl = s.declarations[d]; open(decl.module); if (decl.kind == DeclarationKind::function) out << result(decl.methods[0],p.declarations()[d].methods[0]) << ' ' << decl.name << '(' << params(decl.methods[0],p.declarations()[d].methods[0],true) << ");\n"; else out << "class " << decl.name << ";\n"; close(decl.module); }
        for (auto d : p.declaration_order()) {
            const auto module = s.declarations[d].module;
            const auto& decl = s.declarations[d]; const auto& resolution = p.declarations()[d]; if (decl.kind == DeclarationKind::function) continue;
            open(module); origin(decl.origin); out << "class " << decl.name;
            switch (decl.kind) {
            case DeclarationKind::model: out << " : public gungnir::Model<" << decl.name << '>'; break;
            case DeclarationKind::middleware: out << " : public gungnir::Middleware"; break;
            case DeclarationKind::controller: out << " : public gungnir::Controller"; break;
            case DeclarationKind::migration: out << " : public gungnir::Migration"; break;
            case DeclarationKind::event: out << " : public gungnir::events::Event"; break;
            case DeclarationKind::job: out << " : public gungnir::queue::Job"; break;
            default: break;
            }
            out << " {\npublic:\n";
            std::string primary = "id"; for (const auto& metadata : decl.metadata) if (metadata.name == "primaryKey") primary = s.expressions[metadata.value].text;
            for (auto f : resolution.fields) {
                const auto& field = symbol(f); const bool readonly = field.immutable && field.kind != ResolvedSymbolKind::injection;
                out << (field.visibility == Visibility::private_ ? "private:\n" : field.visibility == Visibility::protected_ ? "protected:\n" : "public:\n");
                out << (readonly ? "const " : "");
                if (decl.kind == DeclarationKind::model) out << (field.name == primary ? "gungnir::PrimaryKey<" : "gungnir::Field<") << type(field.type) << '>';
                else if (field.kind == ResolvedSymbolKind::injection) out << "std::shared_ptr<" << type(field.type) << '>';
                else out << type(field.type);
                out << ' ' << field.name;
                const auto original = std::find_if(decl.fields.begin(),decl.fields.end(),[&](const auto& value){return value.name == field.name;});
                if (original != decl.fields.end() && original->initializer != invalid_id) out << " = " << expr(original->initializer);
                else if (decl.kind == DeclarationKind::model && field.name == primary) { bool incrementing = true; for (const auto& metadata : decl.metadata) if (metadata.name == "incrementing") incrementing = s.expressions[metadata.value].text == "true"; out << '{' << quote(primary) << ',' << (incrementing && p.types()[field.type].name != "string" ? "true" : "false") << '}'; }
                else out << "{}";
                out << ";\n";
            }
            out << "public:\n";
            if (decl.kind == DeclarationKind::model) out << "template<class> friend struct gungnir::model::Generated;\n";
            if (decl.kind != DeclarationKind::model && !resolution.fields.empty()) {
                out << decl.name << '('; bool comma = false; for (auto f : resolution.fields) { const auto& field = symbol(f); if (comma) out << ','; comma = true; if (field.kind == ResolvedSymbolKind::injection) out << "std::shared_ptr<" << type(field.type) << ">"; else out << type(field.type); out << " gnr_" << field.name; }
                out << ") : "; comma = false; for (auto f : resolution.fields) { const auto& field = symbol(f); if (comma) out << ','; comma = true; out << field.name << "(std::move(gnr_" << field.name << "))"; } out << " {}\n";
            }
            if (std::any_of(resolution.fields.begin(),resolution.fields.end(),[&](auto id){return symbol(id).kind == ResolvedSymbolKind::injection;})) {
                out << "static std::shared_ptr<" << decl.name << "> make(gungnir::Container& container";
                for (auto f : resolution.fields) if (symbol(f).kind != ResolvedSymbolKind::injection) out << ',' << type(symbol(f).type) << " gnr_" << symbol(f).name;
                out << ") { return std::make_shared<" << decl.name << ">(";
                bool comma = false; for (auto f : resolution.fields) { if (comma) out << ','; comma = true; const auto& field = symbol(f); if (field.kind == ResolvedSymbolKind::injection) out << "container.resolve<" << type(field.type) << ">()"; else out << "std::move(gnr_" << field.name << ')'; } out << "); }\n";
            }
            if (decl.kind == DeclarationKind::model && std::none_of(decl.metadata.begin(),decl.metadata.end(),[](const auto& metadata){return metadata.name == "table";})) {
                std::string table; for (std::size_t i = 0; i < decl.name.size(); ++i) { const auto c = decl.name[i]; if (c >= 'A' && c <= 'Z') { if (i) table += '_'; table += static_cast<char>(c+32); } else table += c; }
                if (table.ends_with("y") && table.size() > 1 && std::string{"aeiou"}.find(table[table.size()-2]) == std::string::npos) { table.pop_back(); table += "ies"; } else if (table.ends_with("s") || table.ends_with("x") || table.ends_with("ch") || table.ends_with("sh")) table += "es"; else table += 's';
                out << "inline static constexpr gungnir::Table table{" << quote(table) << "};\n";
            }
            for (const auto& metadata : decl.metadata) {
                const auto& value = s.expressions[metadata.value];
                if (metadata.name == "table" || metadata.name == "connection") out << "inline static constexpr gungnir::" << (metadata.name == "table" ? "Table" : "Connection") << ' ' << metadata.name << '{' << quote(value.text) << "};\n";
                else if (metadata.name == "fillable") { out << "inline static constexpr auto fillable = gungnir::Fillable{"; for (std::size_t i = 0; i < value.operands.size(); ++i) { if (i) out << ','; out << quote(s.expressions[value.operands[i]].text); } out << "};\n"; }
                else if (metadata.name == "hidden" || metadata.name == "visible") { out << "inline static constexpr std::array<std::string_view," << value.operands.size() << "> " << metadata.name << "{"; for (auto id : value.operands) out << quote(s.expressions[id].text) << ','; out << "};\n"; }
                else if (metadata.name == "primaryKey") out << "inline static constexpr std::string_view primaryKey = " << quote(value.text) << ";\n";
                else if (metadata.name == "softDeletes") { if (value.text == "true") out << "inline static constexpr gungnir::SoftDeletes soft_deletes{};\n"; }
                else if (metadata.name == "casts") { out << "inline static constexpr std::array<std::pair<std::string_view,std::string_view>," << value.operands.size() << "> casts{{"; for (std::size_t i = 0; i < value.operands.size(); ++i) out << '{' << quote(value.argument_names[i]) << ',' << quote(s.expressions[value.operands[i]].text) << "},"; out << "}};\n"; }
                else out << "inline static constexpr auto " << metadata.name << " = " << expr(metadata.value) << ";\n";
            }
            if (decl.kind == DeclarationKind::event || decl.kind == DeclarationKind::job) out << "inline static constexpr std::string_view event_name = " << quote(s.modules[module].name.empty() ? decl.name : s.modules[module].name + "." + decl.name) << ";\nstd::string_view name() const noexcept override { return event_name; }\n";
            for (std::size_t m = 0; m < decl.methods.size(); ++m) { const auto& method = decl.methods[m]; out << (method.visibility == Visibility::private_ ? "private:\n" : method.visibility == Visibility::protected_ ? "protected:\n" : "public:\n") << result(method,resolution.methods[m]) << ' ' << method.name << '(' << params(method,resolution.methods[m],true) << ')' << (decl.kind == DeclarationKind::migration && (method.name == "up" || method.name == "down") ? " override" : "") << ";\n"; }
            out << "public:\n";
            if (decl.kind == DeclarationKind::listener) {
                const auto it = std::find_if(decl.methods.begin(),decl.methods.end(),[](const auto& m){return m.name == "handle";}); const auto index = static_cast<std::size_t>(it-decl.methods.begin()); const auto event = type(symbol(resolution.methods[index].symbol).parameters[0]);
                out << "static void register_listener(gungnir::events::Dispatcher& dispatcher, std::shared_ptr<" << decl.name << "> listener, int priority = 0) { (void)dispatcher." << (it->asynchronous ? "listen_async" : "listen") << "(std::string{" << event << "::event_name}, [listener](const gungnir::events::Event& event) " << (it->asynchronous ? "-> gungnir::Task<void> " : "") << "{ " << (it->asynchronous ? "co_await " : "") << "listener->handle(dynamic_cast<const " << event << "&>(event)); },priority); }\n";
            }
            if (decl.kind == DeclarationKind::policy) {
                out << "static void register_policy(gungnir::auth::ResourceAuthorization& authorization, std::shared_ptr<" << decl.name << "> policy) {\n";
                for (std::size_t m = 0; m < decl.methods.size(); ++m) { const auto& method = decl.methods[m]; const auto& fn = symbol(resolution.methods[m].symbol); if (fn.parameters.size() == 2 && !method.asynchronous && method.visibility == Visibility::public_) out << "authorization.define<" << type(fn.parameters[0]) << ',' << type(fn.parameters[1]) << ">(" << quote(method.name) << ",[policy](const " << type(fn.parameters[0]) << "& actor,const " << type(fn.parameters[1]) << "& resource) { return policy->" << method.name << "(actor,resource); });\n"; }
                out << "}\n";
            }
            if (decl.kind == DeclarationKind::notification) {
                const auto it = std::find_if(decl.methods.begin(),decl.methods.end(),[](const auto& m){return m.name == "via";}); const auto index = static_cast<std::size_t>(it-decl.methods.begin()); const auto& fn = symbol(resolution.methods[index].symbol);
                out << "inline static constexpr std::string_view notification_name = " << quote(s.modules[module].name.empty() ? decl.name : s.modules[module].name + "." + decl.name) << ";\n";
                if (fn.parameters.size() == 1) out << "auto bind(" << type(fn.parameters[0]) << " recipient) const { return gungnir::language::runtime::BoundNotification<" << decl.name << ',' << type(fn.parameters[0]) << ">{*this,std::move(recipient)}; }\n";
            }
            if (decl.kind == DeclarationKind::mail) {
                out << "gungnir::mail::Message message() { gungnir::mail::Message value; value.subject(subject());";
                for (const auto& method : decl.methods) { if (method.name == "text" || method.name == "html") out << "value." << method.name << '(' << method.name << "());"; if (method.name == "content") out << "value.html(std::string{content().body()});"; } out << "return value; }\n";
            }
            if (decl.kind == DeclarationKind::job) {
                out << "std::string payload() const override { return gungnir::Json::object({"; for (auto f : resolution.fields) if (symbol(f).kind != ResolvedSymbolKind::injection) out << '{' << quote(symbol(f).name) << ",gungnir::http::make_json(" << symbol(f).name << ")},"; out << "}).dump(); }\n";
                const bool injectable = std::any_of(resolution.fields.begin(),resolution.fields.end(),[&](auto id){return symbol(id).kind == ResolvedSymbolKind::injection;});
                out << "static " << decl.name << " from_payload(std::string_view text" << (injectable ? ", gungnir::Container& container" : "") << ") { auto value = gungnir::Json::parse(text); return " << decl.name << '(';
                bool comma = false;
                for (auto f : resolution.fields) { if (comma) out << ','; comma = true; const auto& field = symbol(f); if (field.kind == ResolvedSymbolKind::injection) out << "container.resolve<" << type(field.type) << ">()"; else out << "gungnir::language::runtime::required<" << type(field.type) << ">(value," << quote(field.name) << ')'; }
                out << "); }\n";
                const auto it = std::find_if(decl.methods.begin(),decl.methods.end(),[](const auto& m){return m.name == "handle";});
                out << "static void register_job(gungnir::queue::Worker& worker" << (injectable ? ", gungnir::Container& container" : "") << ") { worker.handle(std::string{event_name},[" << (injectable ? "&container" : "") << "](std::string_view payload) { auto job = from_payload(payload" << (injectable ? ",container" : "") << "); " << (it->asynchronous ? "gungnir::language::runtime::wait(job.handle());" : "job.handle();") << " }); }\n";
                if (injectable) {
                    out << "static void register_job(gungnir::queue::Worker& worker, std::weak_ptr<gungnir::Container> owner) { worker.handle(std::string{event_name},[owner](std::string_view payload) { auto container = owner.lock(); if (!container) throw std::logic_error(\"Job application is no longer available\"); auto job = from_payload(payload,*container); " << (it->asynchronous ? "gungnir::language::runtime::wait(job.handle());" : "job.handle();") << " }); }\n";
                }

            }
            out << "};\n"; close(module);
        }
        for (std::size_t d = 0; d < s.declarations.size(); ++d) if (s.declarations[d].kind == DeclarationKind::model) {
            const auto qualified = symbol(p.declarations()[d].symbol).cpp_name;
            out << "namespace gungnir::model { template<> struct Generated<" << qualified << "> { inline static constexpr auto attributes = std::tuple{";
            for (auto f : p.declarations()[d].fields) out << "attribute(" << quote(symbol(f).name) << ",&" << qualified << "::" << symbol(f).name << "),";
            out << "}; inline static constexpr auto relations = std::tuple{}; }; }\n";
        }
        return out.str();
    }
};
}
CppIrProject CppIrLowerer::lower(
    const ValidatedProject& project,
    bool line_directives
) const {
    CppIrProject result;

    result.support.push_back(CppIrSupportBlock{
        CppIrSupportKind::interface_,
        CppIrLoweringRenderer{
            project,
            line_directives
        }.declarations()
    });

    result.support.push_back(CppIrSupportBlock{
        CppIrSupportKind::header_interface,
        CppIrLoweringRenderer{
            project,
            false
        }.declarations()
    });

    CppIrLoweringRenderer{
        project,
        line_directives
    }.lower_structural_functions(result);

    return result;
}

std::string dump_cpp_ir(const CppIrProject& project) {
    std::ostringstream out;

    out << "cpp-ir structural\n";

    for (std::size_t i = 0; i < project.functions.size(); ++i) {
        const auto& function = project.functions[i];
        out << "function " << i << ' ';
        if (!function.module.empty()) {
            out << function.module << "::";
        }
        if (!function.owner.empty()) {
            out << function.owner << "::";
        }
        out << function.name << " -> "
            << function.result.spelling
            << (function.coroutine ? " coroutine" : "")
            << '\n';

        for (auto statement : function.body) {
            out << "  statement " << statement
                << ' ' << static_cast<int>(
                    project.statements.at(statement).kind
                )
                << '\n';
        }
    }

    for (std::size_t i = 0; i < project.expressions.size(); ++i) {
        const auto& expression = project.expressions[i];
        out << "expression " << i
            << ' ' << static_cast<int>(expression.kind)
            << " : " << expression.type.spelling
            << '\n';
    }

    for (const auto& unit : project.units) {
        out << "unit " << unit.module
            << " functions";
        for (auto function : unit.functions) {
            out << ' ' << function;
        }
        out << '\n';
    }

    return out.str();
}

}
