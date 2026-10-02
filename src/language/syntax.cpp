#include <gungnir/language/compiler.hpp>
#include <gungnir/language/lexer.hpp>
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cmath>
#include <limits>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace gungnir::language {
namespace {
struct ParseFailure {};
class Reader {
public:
    static constexpr std::size_t max_nesting_depth = 256;

    struct NestingGuard {
        Reader& reader;

        explicit NestingGuard(Reader& value) : reader(value) {
            if (reader.nesting_depth >= max_nesting_depth) {
                reader.error(
                    "Maximum syntax nesting depth exceeded",
                    "GNR2004"
                );
            }
            ++reader.nesting_depth;
        }

        ~NestingGuard() {
            --reader.nesting_depth;
        }

        NestingGuard(const NestingGuard&) = delete;
        NestingGuard& operator=(const NestingGuard&) = delete;
    };

    SyntaxResult result;
    std::vector<Token> tokens;
    std::size_t at = 0;
    std::size_t nesting_depth = 0;
    std::string file;
    Reader(std::string_view source, std::string file, std::string module) : file(std::move(file)) {
        auto input = Lexer{source}.tokenize(&result.diagnostics, this->file, true);
        static const std::unordered_set<std::string> pairs{
            "::", "=>", "??", "?.", "==", "!=", "<=", ">=", "&&", "||", "++", "--", "+=", "-=", "*=", "/="};
        for (std::size_t i = 0; i < input.size(); ++i) {
            if (input[i].trivia()) continue;
            auto token = input[i];
            if (i + 1 < input.size() && token.kind == TokenKind::symbol &&
                input[i + 1].kind == TokenKind::symbol &&
                input[i + 1].offset == token.offset + token.lexeme.size() &&
                pairs.contains(token.lexeme + input[i + 1].lexeme)) token.lexeme += input[++i].lexeme;
            tokens.push_back(std::move(token));
        }
        result.project.modules.push_back(ModuleSyntax{origin(), std::move(module), {}, {}});
    }
    const Token& peek(std::size_t offset = 0) const { return tokens[std::min(at + offset, tokens.size() - 1)]; }
    bool is(std::string_view word) const { return peek().lexeme == word; }
    bool end() const { return peek().kind == TokenKind::end; }
    Origin origin() const { return {file, peek().line, peek().column, peek().offset, peek().offset + peek().lexeme.size()}; }
    void error(std::string message, std::string code = "GNR2001") {
        Diagnostic diagnostic{
            DiagnosticLevel::error,
            {file, peek().line, peek().column},
            std::move(message),
            std::move(code),
            {}
        };
        diagnostic.span.begin_offset = peek().offset;
        diagnostic.span.end_offset =
            peek().offset + std::max<std::size_t>(1, peek().lexeme.size());
        diagnostic.span.valid = true;
        result.diagnostics.push_back(std::move(diagnostic));
        throw ParseFailure{};
    }
    bool take(std::string_view word) { if (!is(word)) return false; ++at; return true; }
    void need(std::string_view word) { if (!take(word)) error("Expected '" + std::string{word} + "'"); }
    std::string name() {
        static const std::unordered_set<std::string> reserved{
            "return", "if", "else", "while", "for", "in", "const", "let", "public", "private", "protected",
            "true", "false", "null", "async", "await", "function", "module", "import", "export", "inject"};
        if (!peek().word() || reserved.contains(peek().lexeme)) error("Expected an identifier");
        return tokens[at++].lexeme;
    }
    std::string module_name() {
        std::string value = name();
        while (take(".")) value += "." + name();
        return value;
    }
    TypeSyntax type() {
        NestingGuard guard{*this};
        TypeSyntax value; value.origin = origin(); value.name = name();
        while (take("::")) value.name += "::" + name();
        if (take("<")) {
            do { value.arguments.push_back(type()); } while (take(","));
            need(">");
        }
        value.optional = take("?");
        finish(value.origin);
        return value;
    }
    std::vector<ParameterSyntax> parameters(bool inferred = false) {
        std::vector<ParameterSyntax> values;
        need("(");
        if (!is(")")) do {
            ParameterSyntax value; value.origin = origin();
            if (inferred && (peek(1).lexeme == "," || peek(1).lexeme == ")")) value.name = name();
            else { value.type = type(); value.name = name(); }
            if (take("=")) value.default_value = expression();
            finish(value.origin); values.push_back(std::move(value));
        } while (take(","));
        need(")");
        return values;
    }
    void finish(Origin& value) const {
        if (at) value.end = tokens[at - 1].offset + tokens[at - 1].lexeme.size();
    }
    SyntaxId add(SyntaxExpression node) {
        if (!node.operands.empty()) {
            const auto& first = result.project.expressions[node.operands.front()].origin;
            if (first.begin < node.origin.begin) { node.origin.begin = first.begin; node.origin.line = first.line; node.origin.column = first.column; }
        }
        finish(node.origin);
        result.project.expressions.push_back(std::move(node)); return result.project.expressions.size() - 1; }
    SyntaxId add(SyntaxStatement node) { finish(node.origin); result.project.statements.push_back(std::move(node)); return result.project.statements.size() - 1; }
    static int precedence(std::string_view op) {
        if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=") return 1;
        if (op == "??") return 2;
        if (op == "||") return 3;
        if (op == "&&") return 4;
        if (op == "==" || op == "!=") return 5;
        if (op == "<" || op == ">" || op == "<=" || op == ">=") return 6;
        if (op == "+" || op == "-") return 7;
        if (op == "*" || op == "/" || op == "%") return 8;
        return 0;
    }
    static void utf8(std::string& value, std::uint32_t code) {
        if (code <= 0x7f) value += static_cast<char>(code);
        else if (code <= 0x7ff) { value += static_cast<char>(0xc0 | (code >> 6)); value += static_cast<char>(0x80 | (code & 63)); }
        else if (code <= 0xffff) { value += static_cast<char>(0xe0 | (code >> 12)); value += static_cast<char>(0x80 | ((code >> 6) & 63)); value += static_cast<char>(0x80 | (code & 63)); }
        else { value += static_cast<char>(0xf0 | (code >> 18)); value += static_cast<char>(0x80 | ((code >> 12) & 63)); value += static_cast<char>(0x80 | ((code >> 6) & 63)); value += static_cast<char>(0x80 | (code & 63)); }
    }
    std::string string_value(std::string_view text) {
        if (text.starts_with("R\"")) {
            const auto opening = text.find('(');
            const auto delimiter = text.substr(2, opening - 2);
            return std::string{text.substr(opening + 1, text.size() - opening - delimiter.size() - 3)};
        }
        std::string value;
        for (std::size_t i = 1; i + 1 < text.size(); ++i) {
            char c = text[i];
            if (c != '\\') { value += c; continue; }
            c = text[++i];
            switch (c) {
            case 'n': value += '\n'; break; case 'r': value += '\r'; break;
            case 't': value += '\t'; break; case 'b': value += '\b'; break;
            case 'f': value += '\f'; break; case '0': value += '\0'; break;
            case '\\': case '\'': case '"': case '/': value += c; break;
            case 'u': case 'U': {
                const std::size_t count = c == 'u' ? 4 : 8;
                if (i + count >= text.size() - 1) error("Incomplete Unicode escape", "GNR2002");
                std::uint32_t code = 0;
                const auto digits = text.substr(i + 1, count);
                auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), code, 16);
                if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
                    error("Invalid Unicode scalar escape", "GNR2002");
                utf8(value, code); i += count; break;
            }
            default: error("Unsupported string escape", "GNR2002");
            }
        }
        return value;
    }
    SyntaxId primary() {
        const auto where = origin();
        if (is("await") || is("!") || is("-") || is("+")) {
            const auto op = tokens[at++].lexeme;
            if (op == "-" && peek().kind == TokenKind::number) {
                auto literal = peek().lexeme; std::erase(literal,'_');
                while (literal.ends_with("L") || literal.ends_with("l")) literal.pop_back();
                if (literal == "9223372036854775808" || literal == "0x8000000000000000" || literal == "0X8000000000000000") { ++at; SyntaxExpression value{SyntaxExpressionKind::literal,where,"(-9223372036854775807LL - 1LL)"}; value.literal_type = "int"; return add(std::move(value)); }
            }

            SyntaxExpression node{op == "await" ? SyntaxExpressionKind::await_ : SyntaxExpressionKind::unary, where, op};
            node.operands.push_back(expression(9)); return add(std::move(node));
        }
        if (is("(")) {
            std::size_t close = at, depth = 0;
            for (; close < tokens.size(); ++close) {
                if (tokens[close].lexeme == "(") ++depth;
                if (tokens[close].lexeme == ")" && --depth == 0) break;
            }
            if (close + 1 < tokens.size() && tokens[close + 1].lexeme == "=>") {
                SyntaxExpression node{SyntaxExpressionKind::lambda, where};
                node.parameters = parameters(true); need("=>");
                if (is("{")) node.body = block();
                else { SyntaxStatement statement{SyntaxStatementKind::return_, origin()}; statement.expression = expression(); node.body.push_back(add(std::move(statement))); }
                return add(std::move(node));
            }
            need("("); const auto value = expression(); need(")"); return value;
        }
        if (take("[")) {
            SyntaxExpression node{SyntaxExpressionKind::list, where};
            if (!is("]")) do { node.operands.push_back(expression()); } while (take(",") && !is("]"));
            need("]"); return add(std::move(node));
        }
        if (take("{")) {
            SyntaxExpression node{SyntaxExpressionKind::object, where};
            if (!is("}")) do {
                if (peek().kind != TokenKind::string_literal && !peek().word()) error("Object keys must be strings or names");
                node.argument_names.push_back(peek().kind == TokenKind::string_literal ? string_value(tokens[at++].lexeme) : name()); need(":"); node.operands.push_back(expression());
            } while (take(",") && !is("}"));
            need("}"); return add(std::move(node));
        }
        if (peek().kind == TokenKind::string_literal) {
            SyntaxExpression node{SyntaxExpressionKind::literal, where}; node.literal_type = "string"; node.text = string_value(tokens[at++].lexeme); return add(std::move(node));
        }
        if (peek().kind == TokenKind::number) {
            SyntaxExpression node{SyntaxExpressionKind::literal, where}; node.text = tokens[at++].lexeme; std::erase(node.text, '_');
            node.literal_type = node.text.starts_with("0x") || node.text.starts_with("0X") || (node.text.find_first_of(".eE") == std::string::npos && !node.text.ends_with("f") && !node.text.ends_with("F")) ? "int" : "double";
            try {
                const bool hex = node.text.starts_with("0x") || node.text.starts_with("0X"), binary = node.text.starts_with("0b") || node.text.starts_with("0B");
                if (node.literal_type == "int") {
                    const bool unsigned_value = node.text.find_first_of("uU") != std::string::npos;
                    std::string digits = node.text;
                    while (!digits.empty() && (digits.back() == 'u' || digits.back() == 'U' || digits.back() == 'l' || digits.back() == 'L')) digits.pop_back();
                    std::size_t consumed = 0;
                    auto value = std::stoull(binary || hex ? digits.substr(2) : digits,&consumed,binary ? 2 : hex ? 16 : 10);
                    if (consumed != (binary || hex ? digits.size()-2 : digits.size()) || (!unsigned_value && value > static_cast<unsigned long long>(std::numeric_limits<std::int64_t>::max()))) error("Integer literal is out of range", "GNR2003");
                    node.literal_type = unsigned_value ? "uint64" : "int"; node.text = std::to_string(value) + (unsigned_value ? "ULL" : "LL");
                } else { const auto value = std::stod(node.text); if (!std::isfinite(value)) error("Numeric literal is out of range", "GNR2003"); std::ostringstream normalized; normalized << std::setprecision(17) << value; node.text = normalized.str(); }
            } catch (const std::invalid_argument&) { error("Invalid numeric literal", "GNR2003"); } catch (const std::out_of_range&) { error("Numeric literal is out of range", "GNR2003"); }
            return add(std::move(node));
        }
        if (is("true") || is("false") || is("null")) {
            SyntaxExpression node{SyntaxExpressionKind::literal, where, tokens[at++].lexeme}; node.literal_type = node.text == "null" ? "null" : "bool"; return add(std::move(node));
        }
        SyntaxExpression node{SyntaxExpressionKind::name, where, name()};
        return add(std::move(node));
    }
    SyntaxId expression(int minimum = 1) {
        NestingGuard guard{*this};
        auto left = primary();
        while (!end()) {
            const auto where = origin();
            if (take(".") || take("?.") || take("::")) {
                const auto op = tokens[at - 1].lexeme;
                SyntaxExpression node{SyntaxExpressionKind::member, where, name()}; node.literal_type = op; node.operands = {left}; left = add(std::move(node)); continue;
            }
            if (take("(")) {
                SyntaxExpression node{SyntaxExpressionKind::call, where}; node.operands.push_back(left);
                if (!is(")")) do {
                    std::string parameter;
                    if (peek().word() && peek(1).lexeme == ":") { parameter = name(); need(":"); }
                    node.argument_names.push_back(std::move(parameter)); node.operands.push_back(expression());
                } while (take(",") && !is(")"));
                need(")"); left = add(std::move(node)); continue;
            }
            if (take("[")) { SyntaxExpression node{SyntaxExpressionKind::subscript, where}; node.operands = {left, expression()}; need("]"); left = add(std::move(node)); continue; }
            if (is("++") || is("--")) { SyntaxExpression node{SyntaxExpressionKind::unary, where, "post" + tokens[at++].lexeme}; node.operands = {left}; left = add(std::move(node)); continue; }
            if (is("?") && minimum <= 2) { ++at; SyntaxExpression node{SyntaxExpressionKind::conditional, where}; node.operands = {left, expression()}; need(":"); node.operands.push_back(expression(2)); left = add(std::move(node)); continue; }
            const auto op = peek().lexeme;
            const auto level = precedence(op);
            if (!level || level < minimum) break;
            ++at; SyntaxExpression node{SyntaxExpressionKind::binary, where, op};
            node.operands = {left, expression(level + (level == 1 || op == "??" ? 0 : 1))}; left = add(std::move(node));
        }
        return left;
    }
    std::vector<SyntaxId> block() {
        need("{"); std::vector<SyntaxId> values;
        while (!is("}") && !end()) values.push_back(statement());
        need("}"); return values;
    }
    SyntaxId binding(bool semicolon) {
        SyntaxStatement node{SyntaxStatementKind::binding, origin()}; node.immutable = take("const"); if (!node.immutable) need("let");
        node.name = name(); if (take(":")) node.declared_type = type(); need("="); node.expression = expression(); if (semicolon) need(";"); return add(std::move(node));
    }
    SyntaxId statement() {
        NestingGuard guard{*this};
        if (is("const") || is("let")) return binding(true);
        SyntaxStatement node; node.origin = origin();
        if (is("{")) { node.kind = SyntaxStatementKind::block; node.body = block(); return add(std::move(node)); }
        if (take("return") || take("throw")) { node.kind = tokens[at - 1].lexeme == "return" ? SyntaxStatementKind::return_ : SyntaxStatementKind::throw_; if (!is(";")) node.expression = expression(); need(";"); return add(std::move(node)); }
        if (take("break") || take("continue")) { node.kind = tokens[at - 1].lexeme == "break" ? SyntaxStatementKind::break_ : SyntaxStatementKind::continue_; need(";"); return add(std::move(node)); }
        if (take("if") || take("while")) {
            node.kind = tokens[at - 1].lexeme == "if" ? SyntaxStatementKind::if_ : SyntaxStatementKind::while_;
            need("("); node.expression = expression(); need(")"); node.body = block();
            if (node.kind == SyntaxStatementKind::if_ && take("else")) { if (is("if")) node.alternative.push_back(statement()); else node.alternative = block(); }
            return add(std::move(node));
        }
        if (take("for")) {
            need("(");
            if (peek().word() && peek(1).lexeme == "in") {
                node.kind = SyntaxStatementKind::for_in; node.name = name(); need("in"); node.expression = expression(); need(")"); node.body = block(); return add(std::move(node));
            }
            node.kind = SyntaxStatementKind::for_;
            if (!is(";")) {
                if (is("const") || is("let")) node.parts.push_back(binding(false));
                else { SyntaxStatement initial{SyntaxStatementKind::expression, origin()}; initial.expression = expression(); node.parts.push_back(add(std::move(initial))); }
            }
            need(";"); if (!is(";")) node.expression = expression(); need(";");
            if (!is(")")) { SyntaxStatement update{SyntaxStatementKind::expression, origin()}; update.expression = expression(); node.alternative.push_back(add(std::move(update))); }
            need(")"); node.body = block(); return add(std::move(node));
        }
        node.kind = SyntaxStatementKind::expression; node.expression = expression(); need(";"); return add(std::move(node));
    }
    CallableSyntax callable(DeclarationKind kind, Visibility visibility, bool asynchronous, bool ordinary = false) {
        CallableSyntax value; value.origin = origin(); value.visibility = visibility; value.asynchronous = asynchronous;
        value.asynchronous |= take("async");
        if (peek(1).lexeme == "(") { value.name = name(); value.implicit_result = true; }
        else { value.result = type(); value.asynchronous |= take("async"); value.name = name(); }
        if (ordinary && value.implicit_result) error("Ordinary functions require an explicit return type");
        value.parameters = parameters(); value.body = block();
        if (value.implicit_result) {
            value.result.origin = value.origin;
            if (kind == DeclarationKind::controller || kind == DeclarationKind::middleware) value.result.name = "Response";
            else if (kind == DeclarationKind::policy) value.result.name = "Decision";
            else if (kind == DeclarationKind::migration || kind == DeclarationKind::listener || kind == DeclarationKind::job) value.result.name = "void";
            else if (kind == DeclarationKind::notification && value.name == "via") value.result = {"List", {{"string", {}, false, value.origin}}, false, value.origin};
            else if (kind == DeclarationKind::mail && (value.name == "subject" || value.name == "text" || value.name == "html")) value.result.name = "string";
            else if (kind == DeclarationKind::mail && value.name == "content") value.result.name = "Response";
            else value.result.name = "inferred";
        }
        finish(value.origin);
        return value;
    }
    void declaration() {
        const auto where = origin();
        take("export"); bool asynchronous = take("async");
        if (take("function")) {
            DeclarationSyntax value{where, DeclarationKind::function}; value.methods.push_back(callable(value.kind, Visibility::public_, asynchronous, true)); value.name = value.methods.front().name; finish(value.origin);
            result.project.modules[0].declarations.push_back(result.project.declarations.size()); result.project.declarations.push_back(std::move(value)); return;
        }
        if (asynchronous) error("async at module scope must precede function");
        static const std::unordered_map<std::string, DeclarationKind> kinds{
            {"model", DeclarationKind::model}, {"controller", DeclarationKind::controller}, {"migration", DeclarationKind::migration},
            {"middleware", DeclarationKind::middleware}, {"policy", DeclarationKind::policy}, {"event", DeclarationKind::event},
            {"listener", DeclarationKind::listener}, {"notification", DeclarationKind::notification}, {"mail", DeclarationKind::mail}, {"job", DeclarationKind::job}};
        const auto found = kinds.find(peek().lexeme);
        if (found == kinds.end()) error("Expected a Gungnir declaration; native C++ belongs in compatibility mode");
        ++at; DeclarationSyntax value{where, found->second, name()}; need("{");
        Visibility visibility = Visibility::public_;
        while (!is("}") && !end()) {
            if (take(";")) continue;
            const auto member_origin = origin();
            auto member_visibility = visibility;
            if (take("public") || take("private") || take("protected")) {
                const auto access = tokens[at - 1].lexeme;
                member_visibility = access == "private" ? Visibility::private_ : access == "protected" ? Visibility::protected_ : Visibility::public_;
                if (take(":")) { visibility = member_visibility; continue; }
            }
            if (take("inject")) { FieldSyntax field{member_origin, {}, type(), true}; field.name = name(); field.visibility = member_visibility; need(";"); finish(field.origin); value.fields.push_back(std::move(field)); continue; }
            if (peek().word() && peek(1).lexeme == "=") { MetadataSyntax item{member_origin, name()}; need("="); item.value = expression(); need(";"); value.metadata.push_back(std::move(item)); continue; }
            const auto saved = at;
            bool async = take("async");
            if (peek(1).lexeme == "(") { at = saved; value.methods.push_back(callable(value.kind, member_visibility, false)); continue; }
            auto field_type = type(); async |= take("async"); auto member_name = name();
            if (is("(")) { at = saved; value.methods.push_back(callable(value.kind, member_visibility, false)); continue; }
            if (async) error("Fields cannot be async");
            FieldSyntax field{member_origin, std::move(member_name), std::move(field_type)}; field.visibility = member_visibility;
            if (take("=")) field.initializer = expression(); need(";"); finish(field.origin); value.fields.push_back(std::move(field));
        }
        need("}"); take(";"); finish(value.origin); result.project.modules[0].declarations.push_back(result.project.declarations.size()); result.project.declarations.push_back(std::move(value));
    }
    SyntaxResult run() {
        if (!result.diagnostics.empty()) return std::move(result);
        try {
            if (take("module")) { auto value = module_name(); need(";"); if (!result.project.modules[0].name.empty() && result.project.modules[0].name != value) error("Module declaration does not match its source path", "GNR2101"); result.project.modules[0].name = std::move(value); }
            while (take("import")) { ImportSyntax value{origin(), module_name()}; if (take("as")) value.alias = name(); need(";"); result.project.modules[0].imports.push_back(std::move(value)); }
            while (!end()) declaration();
        } catch (const ParseFailure&) {}
        return std::move(result);
    }
};
}
SyntaxResult SyntaxParser::parse(std::string_view source, std::string file, std::string module) const {
    return Reader{source, std::move(file), std::move(module)}.run();
}
} // namespace gungnir::language
