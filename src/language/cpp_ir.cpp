#include <gungnir/language/cpp_ir.hpp>
#include <gungnir/language/compiler.hpp>
#include <gungnir/language/spec.hpp>
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
                const auto receiver_id = r.receiver_expression == invalid_id ? callee.operands[0] : r.receiver_expression;
                const auto rr = p.expressions()[receiver_id]; const auto& sr = s.expressions[receiver_id];
                const bool is_static = sr.kind == SyntaxExpressionKind::name && rr.symbol != invalid_id && (symbol(rr.symbol).kind == ResolvedSymbolKind::declaration || symbol(rr.symbol).kind == ResolvedSymbolKind::builtin);
                const bool injection = rr.symbol != invalid_id && symbol(rr.symbol).kind == ResolvedSymbolKind::injection;
                if (!target.starts_with("gungnir::") && !target.starts_with("::")) {
                    if (is_static) target = expr(callee.operands[0]) + "::" + target;
                    else {
                        receiver = expr(receiver_id);
                        const auto base = "gungnir::language::runtime::receiver(std::get<0>(gnr_values))";
                        target = (p.types()[rr.type].name == "Session"
                            ? "gungnir::language::runtime::session_receiver(" + std::string{base} + ")."
                            : std::string{base} + (injection ? "->" : ".")) + target;
                    }
                } else if (callable.receives_receiver || (target.starts_with("gungnir::language::runtime::") && (callable.name == "map" || callable.name == "filter" || callable.name == "each" || callable.name == "validate"))) { receiver = expr(receiver_id); receiver_argument = true; }
            }
            // Braced tuple construction fixes evaluation order, including await
            // expressions, before named arguments are reordered for the call.
            std::string tuple = "std::tuple{";
            if (!receiver.empty()) tuple += "gungnir::language::runtime::hold_receiver(" + receiver + ")";
            for (std::size_t i = 1; i < e.operands.size(); ++i) {
                if (tuple.back() != '{') tuple += ',';
                auto value = child(i);
                if (p.types()[p.expressions()[e.operands[i]].type].name == "Callable")
                    value = "gungnir::language::runtime::hold_callable(" + value + ")";
                tuple += value;
            }
            tuple += '}';
            std::string arguments = receiver_argument ? "gungnir::language::runtime::receiver(std::get<0>(gnr_values))" : "";
            TypeId orm_model = invalid_id;
            if (callable.kind == ResolvedSymbolKind::builtin && callee.kind == SyntaxExpressionKind::member) {
                const auto receiver_type = p.expressions()[callee.operands[0]].type;
                if (p.types()[receiver_type].name == "Query" || p.types()[receiver_type].name == "Collection") orm_model = p.types()[receiver_type].arguments[0];
                else for (std::size_t d = 0; d < s.declarations.size(); ++d)
                    if (s.declarations[d].kind == DeclarationKind::model && symbol(p.declarations()[d].symbol).type == receiver_type)
                        orm_model = receiver_type;
            }
            for (std::size_t i = 0; i < r.argument_order.size(); ++i) {
                const auto index = r.argument_order[i];
                if (index == invalid_id && callable.defaults[i] == invalid_id-1) continue;
                if (!arguments.empty()) arguments += ',';
                if (index == invalid_id) arguments += expr(callable.defaults[i]);
                else {
                    auto value = convert("std::get<" + std::to_string(index + (receiver.empty() ? 0 : 1)) + ">(gnr_values)",r.argument_conversions[i],p.expressions()[e.operands[index+1]].type);
                    if (orm_model != invalid_id) {
                        if ((callable.name == "create" || callable.name == "update") && p.types()[p.expressions()[e.operands[index+1]].type].name == "Json")
                            value = "gungnir::language::runtime::attributes<" + type(orm_model) + ">(" + value + ")";
                        if (callable.name == "orderBy" && i == 1) value = "gungnir::language::runtime::sort_direction(" + value + ")";
                        if ((callable.name == "where" || callable.name == "orWhere") && r.argument_order.size() == 3 && i == 1) value = "gungnir::language::runtime::comparison(" + value + ")";
                        if ((callable.name == "whereIn" || callable.name == "whereNotIn") && i == 1) value = "gungnir::language::runtime::attribute_values(" + value + ")";
                        if (callable.name == "paginate" || callable.name == "limit" || callable.name == "offset" || callable.name == "take" || callable.name == "skip" || callable.name == "at" || callable.name == "chunk") value = "gungnir::language::runtime::query_size(" + value + (callable.name == "paginate" || callable.name == "chunk" ? ", true" : "") + ")";
                        if (((callable.name == "where" || callable.name == "orWhere") && i == r.argument_order.size()-1) || ((callable.name == "find" || callable.name == "findOrFail") && !callable.receives_receiver)) value = "gungnir::model::to_value(" + value + ")";
                    }
                    arguments += value;
                }
            }
            auto invocation = target + "(" + arguments + ")";
            if (!callable.asynchronous && p.types()[r.type].name == "string" && !p.types()[r.type].optional) invocation = "gungnir::String(" + invocation + ")";
            // Queries and loaded values must outlive temporary receivers.
            // Schema definitions refer to objects owned by the enclosing
            // Table callback; preserve those references for chained modifiers.
            const auto& result_name = p.types()[r.type].name;
            const bool schema_reference = result_name == "ColumnDefinition" ||
                result_name == "IndexDefinition" || result_name == "ForeignKeyDefinition";
            const auto result_type = callable.asynchronous || schema_reference ? "decltype(auto)" : type(r.type);
            return "([&](auto&& gnr_values) -> " + result_type + " { return " + invocation + "; }(" + tuple + "))";
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
            // Recursive lowering may grow ir.expressions and reallocate its
            // storage. Compute the child first, then reacquire the parent by
            // ID instead of holding a subobject reference across recursion.
            const auto lowered_operand =
                lower_expression(ir, operand);
            ir.expressions[result_id].operands.push_back(
                lowered_operand
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
                const auto lowered_statement =
                    lower_statement(ir, statement);
                ir.expressions[result_id].body.push_back(
                    lowered_statement
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
                ? CppIrStatementKind::co_return_
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
                        CppIrStatementKind::co_return_;
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
    CppIrLoweringRenderer(const ValidatedProject& project,bool emit_lines) : p(project),s(project.syntax()),lines(emit_lines) { for (std::size_t d = 0; d < s.declarations.size(); ++d) if (s.declarations[d].kind == DeclarationKind::model) for (auto f : p.declarations()[d].fields) model_fields.insert(f); }
    void lower_structural_functions(CppIrProject& ir) {
        lower_functions(ir);
    }
    std::string wrap_module(
        std::size_t module,
        std::string code
    ) const {
        const auto name = ns(module);
        if (name.empty()) {
            return code;
        }

        return "namespace " + name + " {\n" +
            code + "}\n";
    }

    std::string origin_prefix(
        const Origin& value
    ) const {
        if (!lines) {
            return {};
        }

        return "#line " + std::to_string(value.line) +
            " " + quote(value.file) + "\n";
    }

    CppIrDeclaration forward_declaration(
        std::size_t id
    ) {
        const auto& declaration = s.declarations[id];
        std::ostringstream block;

        if (
            declaration.kind ==
            DeclarationKind::function
        ) {
            block
                << result(
                    declaration.methods[0],
                    p.declarations()[id].methods[0]
                )
                << ' '
                << declaration.name
                << '('
                << params(
                    declaration.methods[0],
                    p.declarations()[id].methods[0],
                    true
                )
                << ");\n";

            return CppIrDeclaration{
                CppIrDeclarationKind::function_forward,
                s.modules[declaration.module].name,
                declaration.name,
                wrap_module(
                    declaration.module,
                    block.str()
                )
            };
        }

        block << "class " << declaration.name << ";\n";

        return CppIrDeclaration{
            CppIrDeclarationKind::class_forward,
            s.modules[declaration.module].name,
            declaration.name,
            wrap_module(
                declaration.module,
                block.str()
            )
        };
    }

    CppIrDeclaration class_declaration(
        std::size_t id
    ) {
        const auto& declaration = s.declarations[id];
        const auto& resolution = p.declarations()[id];
        const auto module = declaration.module;
        std::ostringstream block;

        block << origin_prefix(declaration.origin)
              << "class " << declaration.name;

        switch (declaration.kind) {
        case DeclarationKind::model:
            block << " : public gungnir::Model<"
                  << declaration.name << '>';
            break;
        case DeclarationKind::middleware:
            block << " : public gungnir::Middleware";
            break;
        case DeclarationKind::controller:
            block << " : public gungnir::Controller";
            break;
        case DeclarationKind::migration:
            block << " : public gungnir::Migration";
            break;
        case DeclarationKind::event:
            block << " : public gungnir::events::Event";
            break;
        case DeclarationKind::job:
            block << " : public gungnir::queue::Job";
            break;
        default:
            break;
        }

        block << " {\npublic:\n";

        std::string primary{"id"};
        for (const auto& metadata : declaration.metadata) {
            if (metadata.name == "primaryKey") {
                primary =
                    s.expressions[metadata.value].text;
            }
        }

        for (auto field_id : resolution.fields) {
            const auto& field = symbol(field_id);
            const bool readonly =
                field.immutable &&
                field.kind !=
                    ResolvedSymbolKind::injection;

            block
                << (
                    field.visibility == Visibility::private_
                        ? "private:\n"
                        : field.visibility ==
                                Visibility::protected_
                            ? "protected:\n"
                            : "public:\n"
                )
                << (readonly ? "const " : "");

            if (
                declaration.kind ==
                DeclarationKind::model
            ) {
                block
                    << (
                        field.name == primary
                            ? "gungnir::PrimaryKey<"
                            : "gungnir::Field<"
                    )
                    << type(field.type)
                    << '>';
            } else if (
                field.kind ==
                ResolvedSymbolKind::injection
            ) {
                block
                    << "std::shared_ptr<"
                    << type(field.type)
                    << '>';
            } else {
                block << type(field.type);
            }

            block << ' ' << field.name;

            const auto original = std::find_if(
                declaration.fields.begin(),
                declaration.fields.end(),
                [&](const auto& value) {
                    return value.name == field.name;
                }
            );

            if (
                original != declaration.fields.end() &&
                original->initializer != invalid_id
            ) {
                block
                    << " = "
                    << expr(original->initializer);
            } else if (
                declaration.kind ==
                    DeclarationKind::model &&
                field.name == primary
            ) {
                bool incrementing = true;
                for (const auto& metadata :
                     declaration.metadata) {
                    if (
                        metadata.name ==
                        "incrementing"
                    ) {
                        incrementing =
                            s.expressions[
                                metadata.value
                            ].text == "true";
                    }
                }

                block
                    << '{'
                    << quote(primary)
                    << ','
                    << (
                        incrementing &&
                        p.types()[field.type].name !=
                            "string"
                            ? "true"
                            : "false"
                    )
                    << '}';
            } else {
                block << "{}";
            }

            block << ";\n";
        }

        block << "public:\n";

        for (const auto& relation : resolution.relationships) {
            const auto& field = symbol(relation.field);
            block << type(field.type) << ' ' << field.cpp_name << '{';
            for (std::size_t k = 0; k < relation.keys.size(); ++k) {
                if (k) block << ',';
                block << quote(relation.keys[k]);
            }
            block << "};\n";
        }

        if (
            declaration.kind ==
            DeclarationKind::model
        ) {
            block
                << "template<class> friend struct "
                   "gungnir::model::Generated;\n";
        }

        if (
            declaration.kind !=
                DeclarationKind::model &&
            !resolution.fields.empty()
        ) {
            block << declaration.name << '(';
            bool comma = false;

            for (auto field_id : resolution.fields) {
                const auto& field = symbol(field_id);

                if (comma) {
                    block << ',';
                }
                comma = true;

                if (
                    field.kind ==
                    ResolvedSymbolKind::injection
                ) {
                    block
                        << "std::shared_ptr<"
                        << type(field.type)
                        << ">";
                } else {
                    block << type(field.type);
                }

                block << " gnr_" << field.name;
            }

            block << ") : ";
            comma = false;

            for (auto field_id : resolution.fields) {
                const auto& field = symbol(field_id);

                if (comma) {
                    block << ',';
                }
                comma = true;

                block
                    << field.name
                    << "(std::move(gnr_"
                    << field.name
                    << "))";
            }

            block << " {}\n";
        }

        if (
            std::any_of(
                resolution.fields.begin(),
                resolution.fields.end(),
                [&](auto field_id) {
                    return symbol(field_id).kind ==
                        ResolvedSymbolKind::injection;
                }
            )
        ) {
            block
                << "static std::shared_ptr<"
                << declaration.name
                << "> make(gungnir::Container& container";

            for (auto field_id : resolution.fields) {
                const auto& field = symbol(field_id);
                if (
                    field.kind !=
                    ResolvedSymbolKind::injection
                ) {
                    block
                        << ','
                        << type(field.type)
                        << " gnr_"
                        << field.name;
                }
            }

            block
                << ") { return std::make_shared<"
                << declaration.name
                << ">(";

            bool comma = false;
            for (auto field_id : resolution.fields) {
                const auto& field = symbol(field_id);
                if (comma) {
                    block << ',';
                }
                comma = true;

                if (
                    field.kind ==
                    ResolvedSymbolKind::injection
                ) {
                    block
                        << "container.resolve<"
                        << type(field.type)
                        << ">()";
                } else {
                    block
                        << "std::move(gnr_"
                        << field.name
                        << ')';
                }
            }

            block << "); }\n";
        }

        if (
            declaration.kind ==
                DeclarationKind::model &&
            std::none_of(
                declaration.metadata.begin(),
                declaration.metadata.end(),
                [](const auto& metadata) {
                    return metadata.name == "table";
                }
            )
        ) {
            std::string table;
            for (
                std::size_t i = 0;
                i < declaration.name.size();
                ++i
            ) {
                const auto value =
                    declaration.name[i];

                if (
                    value >= 'A' &&
                    value <= 'Z'
                ) {
                    if (i) {
                        table += '_';
                    }
                    table += static_cast<char>(
                        value + 32
                    );
                } else {
                    table += value;
                }
            }

            if (
                table.ends_with("y") &&
                table.size() > 1 &&
                std::string{"aeiou"}.find(
                    table[table.size() - 2]
                ) == std::string::npos
            ) {
                table.pop_back();
                table += "ies";
            } else if (
                table.ends_with("s") ||
                table.ends_with("x") ||
                table.ends_with("ch") ||
                table.ends_with("sh")
            ) {
                table += "es";
            } else {
                table += 's';
            }

            block
                << "inline static constexpr "
                   "gungnir::Table table{"
                << quote(table)
                << "};\n";
        }

        for (const auto& metadata :
             declaration.metadata) {
            const auto& value =
                s.expressions[metadata.value];

            if (
                metadata.name == "table" ||
                metadata.name == "connection"
            ) {
                block
                    << "inline static constexpr "
                       "gungnir::"
                    << (
                        metadata.name == "table"
                            ? "Table"
                            : "Connection"
                    )
                    << ' '
                    << metadata.name
                    << '{'
                    << quote(value.text)
                    << "};\n";
            } else if (
                metadata.name == "fillable"
            ) {
                block
                    << "inline static constexpr auto "
                       "fillable = gungnir::Fillable{";

                for (
                    std::size_t i = 0;
                    i < value.operands.size();
                    ++i
                ) {
                    if (i) {
                        block << ',';
                    }

                    block << quote(
                        s.expressions[
                            value.operands[i]
                        ].text
                    );
                }

                block << "};\n";
            } else if (
                metadata.name == "hidden" ||
                metadata.name == "visible"
            ) {
                block
                    << "inline static constexpr "
                       "std::array<std::string_view,"
                    << value.operands.size()
                    << "> "
                    << metadata.name
                    << "{";

                for (auto expression_id :
                     value.operands) {
                    block << quote(
                        s.expressions[
                            expression_id
                        ].text
                    ) << ',';
                }

                block << "};\n";
            } else if (
                metadata.name == "primaryKey"
            ) {
                block
                    << "inline static constexpr "
                       "std::string_view primaryKey = "
                    << quote(value.text)
                    << ";\n";
            } else if (
                metadata.name == "softDeletes"
            ) {
                if (value.text == "true") {
                    block
                        << "inline static constexpr "
                           "gungnir::SoftDeletes "
                           "soft_deletes{};\n";
                }
            } else if (
                metadata.name == "casts"
            ) {
                block
                    << "inline static constexpr "
                       "std::array<std::pair<"
                       "std::string_view,"
                       "std::string_view>,"
                    << value.operands.size()
                    << "> casts{{";

                for (
                    std::size_t i = 0;
                    i < value.operands.size();
                    ++i
                ) {
                    block
                        << '{'
                        << quote(
                            value.argument_names[i]
                        )
                        << ','
                        << quote(
                            s.expressions[
                                value.operands[i]
                            ].text
                        )
                        << "},";
                }

                block << "}};\n";
            } else {
                block
                    << "inline static constexpr auto "
                    << metadata.name
                    << " = "
                    << expr(metadata.value)
                    << ";\n";
            }
        }

        if (
            declaration.kind ==
                DeclarationKind::event ||
            declaration.kind ==
                DeclarationKind::job
        ) {
            block
                << "inline static constexpr "
                   "std::string_view event_name = "
                << quote(
                    s.modules[module].name.empty()
                        ? declaration.name
                        : s.modules[module].name +
                            "." + declaration.name
                )
                << ";\nstd::string_view name() "
                   "const noexcept override { "
                   "return event_name; }\n";
        }

        for (
            std::size_t method_id = 0;
            method_id < declaration.methods.size();
            ++method_id
        ) {
            const auto& method =
                declaration.methods[method_id];

            block
                << (
                    method.visibility ==
                            Visibility::private_
                        ? "private:\n"
                        : method.visibility ==
                                Visibility::protected_
                            ? "protected:\n"
                            : "public:\n"
                )
                << result(
                    method,
                    resolution.methods[method_id]
                )
                << ' '
                << method.name
                << '('
                << params(
                    method,
                    resolution.methods[method_id],
                    true
                )
                << ')'
                << (
                    declaration.kind ==
                            DeclarationKind::migration &&
                    (
                        method.name == "up" ||
                        method.name == "down"
                    )
                        ? " override"
                        : ""
                )
                << ";\n";
        }

        block << "public:\n";

        if (
            declaration.kind ==
            DeclarationKind::listener
        ) {
            const auto it = std::find_if(
                declaration.methods.begin(),
                declaration.methods.end(),
                [](const auto& method) {
                    return method.name == "handle";
                }
            );
            const auto index =
                static_cast<std::size_t>(
                    it - declaration.methods.begin()
                );
            const auto event = type(
                symbol(
                    resolution.methods[index].symbol
                ).parameters[0]
            );

            block
                << "static void register_listener("
                   "gungnir::events::Dispatcher& "
                   "dispatcher, std::shared_ptr<"
                << declaration.name
                << "> listener, int priority = 0) { "
                   "(void)dispatcher."
                << (
                    it->asynchronous
                        ? "listen_async"
                        : "listen"
                )
                << "(std::string{"
                << event
                << "::event_name}, [listener]("
                   "const gungnir::events::Event& "
                   "event) "
                << (
                    it->asynchronous
                        ? "-> gungnir::Task<void> "
                        : ""
                )
                << "{ "
                << (
                    it->asynchronous
                        ? "co_await "
                        : ""
                )
                << "listener->handle(dynamic_cast<const "
                << event
                << "&>(event)); },priority); }\n";
        }

        if (
            declaration.kind ==
            DeclarationKind::policy
        ) {
            block
                << "static void register_policy("
                   "gungnir::auth::ResourceAuthorization& "
                   "authorization, std::shared_ptr<"
                << declaration.name
                << "> policy) {\n";

            for (
                std::size_t method_id = 0;
                method_id <
                    declaration.methods.size();
                ++method_id
            ) {
                const auto& method =
                    declaration.methods[method_id];
                const auto& function = symbol(
                    resolution.methods[method_id].symbol
                );

                if (
                    function.parameters.size() == 2 &&
                    !method.asynchronous &&
                    method.visibility ==
                        Visibility::public_
                ) {
                    block
                        << "authorization.define<"
                        << type(function.parameters[0])
                        << ','
                        << type(function.parameters[1])
                        << ">("
                        << quote(method.name)
                        << ",[policy](const "
                        << type(function.parameters[0])
                        << "& actor,const "
                        << type(function.parameters[1])
                        << "& resource) { return "
                           "policy->"
                        << method.name
                        << "(actor,resource); });\n";
                }
            }

            block << "}\n";
        }

        if (
            declaration.kind ==
            DeclarationKind::notification
        ) {
            const auto it = std::find_if(
                declaration.methods.begin(),
                declaration.methods.end(),
                [](const auto& method) {
                    return method.name == "via";
                }
            );
            const auto index =
                static_cast<std::size_t>(
                    it - declaration.methods.begin()
                );
            const auto& function = symbol(
                resolution.methods[index].symbol
            );

            block
                << "inline static constexpr "
                   "std::string_view "
                   "notification_name = "
                << quote(
                    s.modules[module].name.empty()
                        ? declaration.name
                        : s.modules[module].name +
                            "." + declaration.name
                )
                << ";\n";

            if (function.parameters.size() == 1) {
                block
                    << "auto bind("
                    << type(function.parameters[0])
                    << " recipient) const { return "
                       "gungnir::language::runtime::"
                       "BoundNotification<"
                    << declaration.name
                    << ','
                    << type(function.parameters[0])
                    << ">{*this,std::move(recipient)}; }\n";
            }
        }

        if (
            declaration.kind ==
            DeclarationKind::mail
        ) {
            block
                << "gungnir::mail::Message message() { "
                   "gungnir::mail::Message value; "
                   "value.subject(subject());";

            for (const auto& method :
                 declaration.methods) {
                if (
                    method.name == "text" ||
                    method.name == "html"
                ) {
                    block
                        << "value."
                        << method.name
                        << '('
                        << method.name
                        << "());";
                }

                if (method.name == "content") {
                    block
                        << "value.html(std::string{"
                           "content().body()});";
                }
            }

            block << "return value; }\n";
        }

        if (
            declaration.kind ==
            DeclarationKind::job
        ) {
            block
                << "std::string payload() const override "
                   "{ return gungnir::Json::object({";

            for (auto field_id : resolution.fields) {
                const auto& field = symbol(field_id);
                if (
                    field.kind !=
                    ResolvedSymbolKind::injection
                ) {
                    block
                        << '{'
                        << quote(field.name)
                        << ",gungnir::http::make_json("
                        << field.name
                        << ")},";
                }
            }

            block << "}).dump(); }\n";

            const bool injectable = std::any_of(
                resolution.fields.begin(),
                resolution.fields.end(),
                [&](auto field_id) {
                    return symbol(field_id).kind ==
                        ResolvedSymbolKind::injection;
                }
            );

            block
                << "static "
                << declaration.name
                << " from_payload(std::string_view text"
                << (
                    injectable
                        ? ", gungnir::Container& container"
                        : ""
                )
                << ") { auto value = "
                   "gungnir::Json::parse(text); return "
                << declaration.name
                << '(';

            bool comma = false;
            for (auto field_id : resolution.fields) {
                const auto& field = symbol(field_id);

                if (comma) {
                    block << ',';
                }
                comma = true;

                if (
                    field.kind ==
                    ResolvedSymbolKind::injection
                ) {
                    block
                        << "container.resolve<"
                        << type(field.type)
                        << ">()";
                } else {
                    block
                        << "gungnir::language::runtime::"
                           "required<"
                        << type(field.type)
                        << ">(value,"
                        << quote(field.name)
                        << ')';
                }
            }

            block << "); }\n";

            const auto it = std::find_if(
                declaration.methods.begin(),
                declaration.methods.end(),
                [](const auto& method) {
                    return method.name == "handle";
                }
            );

            block
                << "static void register_job("
                   "gungnir::queue::Worker& worker"
                << (
                    injectable
                        ? ", gungnir::Container& container"
                        : ""
                )
                << ") { worker.handle(std::string{"
                   "event_name},["
                << (injectable ? "&container" : "")
                << "](std::string_view payload) { "
                   "auto job = from_payload(payload"
                << (injectable ? ",container" : "")
                << "); "
                << (
                    it->asynchronous
                        ? "gungnir::language::runtime::"
                          "wait(job.handle());"
                        : "job.handle();"
                )
                << " }); }\n";

            if (injectable) {
                block
                    << "static void register_job("
                       "gungnir::queue::Worker& worker, "
                       "std::weak_ptr<gungnir::Container> "
                       "owner) { worker.handle(std::string{"
                       "event_name},[owner](std::string_view "
                       "payload) { auto container = "
                       "owner.lock(); if (!container) throw "
                       "std::logic_error(\"Job application "
                       "is no longer available\"); auto job "
                       "= from_payload(payload,*container); "
                    << (
                        it->asynchronous
                            ? "gungnir::language::runtime::"
                              "wait(job.handle());"
                            : "job.handle();"
                    )
                    << " }); }\n";
            }
        }

        block << "};\n";

        return CppIrDeclaration{
            CppIrDeclarationKind::class_definition,
            s.modules[module].name,
            declaration.name,
            wrap_module(module, block.str())
        };
    }

    CppIrDeclaration model_metadata(
        std::size_t id
    ) {
        const auto& declaration = s.declarations[id];
        const auto qualified = symbol(
            p.declarations()[id].symbol
        ).cpp_name;
        std::ostringstream block;

        block
            << "namespace gungnir::model { "
               "template<> struct Generated<"
            << qualified
            << "> { inline static constexpr auto "
               "attributes = std::tuple{";

        for (auto field_id :
             p.declarations()[id].fields) {
            block
                << "attribute("
                << quote(symbol(field_id).name)
                << ",&"
                << qualified
                << "::"
                << symbol(field_id).name
                << "),";
        }

        block << "}; inline static constexpr auto relations = std::tuple{";
        for (const auto& relation : p.declarations()[id].relationships) {
            const auto& field = symbol(relation.field);
            block << "relation(" << quote(field.name) << ",&" << qualified
                  << "::" << field.cpp_name << "),";
        }
        block << "}; }; }\n";

        return CppIrDeclaration{
            CppIrDeclarationKind::model_metadata,
            s.modules[declaration.module].name,
            declaration.name,
            block.str()
        };
    }

    std::vector<CppIrDeclaration> declarations() {
        std::vector<CppIrDeclaration> result;

        std::string preamble =
            "// Generated from a validated Gungnir program.\n"
            "#include <gungnir/language/runtime.hpp>\n"
            "#include <optional>\n"
            "#include <tuple>\n"
            "static_assert("
            "gungnir::language::compiler_contract_version == \"";
        preamble += compiler_contract_version;
        preamble +=
            "\", \"Generated Gungnir source requires compiler/runtime "
            "contract ";
        preamble += compiler_contract_version;
        preamble += "\");\n";

        result.push_back(CppIrDeclaration{
            CppIrDeclarationKind::preamble,
            {},
            {},
            std::move(preamble)
        });

        for (
            std::size_t id = 0;
            id < s.declarations.size();
            ++id
        ) {
            result.push_back(
                forward_declaration(id)
            );
        }

        for (auto id : p.declaration_order()) {
            if (
                s.declarations[id].kind ==
                DeclarationKind::function
            ) {
                continue;
            }

            result.push_back(
                class_declaration(id)
            );
        }

        for (
            std::size_t id = 0;
            id < s.declarations.size();
            ++id
        ) {
            if (
                s.declarations[id].kind ==
                DeclarationKind::model
            ) {
                result.push_back(
                    model_metadata(id)
                );
            }
        }

        return result;
    }

};
}
CppIrProject CppIrLowerer::lower(
    const ValidatedProject& project,
    bool line_directives
) const {
    CppIrProject result;

    result.interface_declarations =
        CppIrLoweringRenderer{
            project,
            line_directives
        }.declarations();

    result.header_declarations =
        CppIrLoweringRenderer{
            project,
            false
        }.declarations();

    CppIrLoweringRenderer{
        project,
        line_directives
    }.lower_structural_functions(result);

    return result;
}

std::string dump_cpp_ir(const CppIrProject& project) {
    std::ostringstream out;

    out << "cpp-ir structural\n";

    for (std::size_t i = 0; i < project.interface_declarations.size(); ++i) {
        const auto& declaration = project.interface_declarations[i];
        out << "declaration " << i
            << ' ' << static_cast<int>(declaration.kind);
        if (!declaration.module.empty()) {
            out << " module=" << declaration.module;
        }
        if (!declaration.name.empty()) {
            out << " name=" << declaration.name;
        }
        out << '\n';
    }

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
