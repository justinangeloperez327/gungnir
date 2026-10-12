#include "spear.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <limits>
#include <set>

namespace gungnir::view::detail {
namespace {
bool word_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool word_part(char c) { return word_start(c) || (c >= '0' && c <= '9'); }
bool blank(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

struct Syntax {
    Template& file;
    std::vector<std::size_t> lines{0};
    std::size_t count{0};
    explicit Syntax(Template& input) : file(input) {
        for (std::size_t i = 0; i < input.source.size(); ++i)
            if (input.source[i] == '\n') lines.push_back(i + 1);
    }
    Location location(std::size_t offset) const {
        auto line = std::upper_bound(lines.begin(), lines.end(), offset) - lines.begin();
        return {static_cast<std::size_t>(line), offset - lines[line - 1] + 1};
    }
    [[noreturn]] void error(std::size_t offset, String message) const { fail(file, location(offset), std::move(message)); }
    void node(std::size_t offset, unsigned depth) {
        if (depth > 64) error(offset, "Spear syntax nesting limit exceeded");
        if (++count > 100000) error(offset, "Spear syntax node limit exceeded");
    }
};

struct Token {
    enum class Kind { end, identifier, string, number, symbol };
    Kind kind{Kind::end};
    String text;
    std::size_t offset{0};
};
class ExpressionParser {
public:
    ExpressionParser(Syntax& syntax, std::string_view source, std::size_t start)
        : syntax_(syntax), source_(source), start_(start) { next(); }
    Expr expression() {
        auto result = conditional(0);
        if (token_.kind != Token::Kind::end) error("Unexpected token in Spear expression");
        return result;
    }
    std::vector<Expr> arguments() {
        std::vector<Expr> result;
        if (token_.kind == Token::Kind::end) return result;
        do { result.push_back(conditional(0)); } while (take(","));
        if (token_.kind != Token::Kind::end) error("Expected ',' between directive arguments");
        return result;
    }
    void loop(Node& node) {
        node.expressions.push_back(conditional(0));
        if (token_.kind != Token::Kind::identifier || token_.text != "as") error("Loop requires 'collection as item'");
        next();
        auto first = variable();
        if (take("=>")) { node.key = std::move(first); node.variable = variable(); }
        else node.variable = std::move(first);
        if (node.key == node.variable) error("Loop key and value must use different names");
        if (token_.kind != Token::Kind::end) error("Unexpected token after loop variable");
    }
private:
    [[noreturn]] void error(String message) const { syntax_.error(token_.offset, std::move(message)); }
    void next() {
        while (position_ < source_.size() && blank(source_[position_])) ++position_;
        token_ = {Token::Kind::end, {}, start_ + position_};
        if (position_ == source_.size()) return;
        const char c = source_[position_];
        if (word_start(c) || c == '$') {
            token_.kind = Token::Kind::identifier;
            if (c == '$') {
                ++position_;
                if (position_ == source_.size() || !word_start(source_[position_])) error("Expected a variable after '$'");
            }
            auto start = position_++;
            while (position_ < source_.size() && word_part(source_[position_])) ++position_;
            token_.text = source_.substr(start, position_ - start);
            return;
        }
        if (c == '\'' || c == '"') {
            token_.kind = Token::Kind::string;
            ++position_;
            while (position_ < source_.size()) {
                char value = source_[position_++];
                if (value == c) return;
                if (value == '\\') {
                    if (position_ == source_.size()) error("Unclosed string escape");
                    value = source_[position_++];
                    switch (value) {
                    case 'n': value = '\n'; break;
                    case 'r': value = '\r'; break;
                    case 't': value = '\t'; break;
                    case '\\': case '\'': case '"': break;
                    default: error("Unsupported string escape");
                    }
                }
                token_.text.push_back(value);
            }
            error("Unclosed Spear string literal");
        }
        if (c >= '0' && c <= '9') {
            token_.kind = Token::Kind::number;
            auto start = position_++;
            while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') ++position_;
            if (position_ < source_.size() && source_[position_] == '.') {
                ++position_;
                while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') ++position_;
            }
            if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
                ++position_;
                if (position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-')) ++position_;
                while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') ++position_;
            }
            token_.text = source_.substr(start, position_ - start);
            return;
        }
        token_.kind = Token::Kind::symbol;
        for (std::string_view symbol : {"===", "!==", "??", "&&", "||", "==", "!=", "<=", ">=", "=>", "->"}) {
            if (source_.substr(position_).starts_with(symbol)) {
                position_ += symbol.size(); token_.text = symbol; return;
            }
        }
        token_.text.assign(1, c); ++position_;
    }
    bool take(std::string_view symbol) {
        if (token_.text != symbol || token_.kind != Token::Kind::symbol) return false;
        next(); return true;
    }
    void expect(std::string_view symbol) { if (!take(symbol)) error("Expected '" + String{symbol} + "'"); }
    String variable() {
        if (token_.kind != Token::Kind::identifier) error("Expected a loop variable name");
        auto name = token_.text;
        if (name == "loop" || name == "this" || name == "true" || name == "false" || name == "null") error("Reserved loop variable name");
        next(); return name;
    }
    std::shared_ptr<Expression> make(Expression::Kind kind, std::size_t offset, unsigned depth) {
        syntax_.node(offset, depth);
        auto result = std::make_shared<Expression>(); result->kind = kind; result->location = syntax_.location(offset); return result;
    }
    Expr seal(std::shared_ptr<Expression> node) {
        for (const auto& child : node->children) node->height = std::max(node->height, child->height + 1);
        if (node->height > 64) fail(syntax_.file, node->location, "Spear expression AST depth limit exceeded");
        return node;
    }
    static int precedence(std::string_view op) {
        if (op == "??") return 1;
        if (op == "||") return 2;
        if (op == "&&") return 3;
        if (op == "==" || op == "!=" || op == "===" || op == "!==") return 4;
        if (op == "<" || op == ">" || op == "<=" || op == ">=") return 5;
        return 0;
    }
    Expr conditional(unsigned depth) {
        auto result = binary(1, depth + 1);
        if (take("?")) {
            auto node = make(Expression::Kind::conditional, token_.offset, depth);
            node->children.push_back(result); node->children.push_back(conditional(depth + 1));
            expect(":"); node->children.push_back(conditional(depth + 1)); result = seal(node);
        }
        return result;
    }
    Expr binary(int minimum, unsigned depth) {
        auto result = unary(depth + 1);
        while (token_.kind == Token::Kind::symbol && precedence(token_.text) >= minimum) {
            auto op = token_;
            next(); auto node = make(Expression::Kind::binary, op.offset, depth);
            node->text = op.text;
            node->children = {result, binary(precedence(op.text) + (op.text == "??" ? 0 : 1), depth + 1)};
            result = seal(node);
        }
        return result;
    }
    Expr unary(unsigned depth) {
        if (token_.text == "!" || token_.text == "-") {
            auto op = token_; next(); auto node = make(Expression::Kind::unary, op.offset, depth);
            node->text = op.text; node->children.push_back(unary(depth + 1)); return seal(node);
        }
        auto result = primary(depth + 1);
        while (token_.text == "." || token_.text == "->" || token_.text == "[" || token_.text == "(") {
            auto op = token_; next();
            if (op.text == "." || op.text == "->") {
                auto node = make(Expression::Kind::member, op.offset, depth);
                if (token_.kind != Token::Kind::identifier) error("Expected a property name");
                node->text = token_.text; node->children.push_back(result); next(); result = seal(node);
            } else if (op.text == "[") {
                auto node = make(Expression::Kind::index, op.offset, depth);
                node->children = {result, conditional(depth + 1)}; expect("]"); result = seal(node);
            } else {
                if (result->kind != Expression::Kind::variable) error("Only registered view helpers are callable");
                auto node = make(Expression::Kind::helper, op.offset, depth); node->text = result->text;
                if (!take(")")) {
                    do { node->children.push_back(conditional(depth + 1)); } while (take(","));
                    expect(")");
                }
                result = seal(node);
            }
        }
        return result;
    }
    Expr primary(unsigned depth) {
        auto current = token_;
        auto node = make(Expression::Kind::literal, current.offset, depth);
        if (current.kind == Token::Kind::string) { node->value = current.text; next(); return node; }
        if (current.kind == Token::Kind::number) {
            if (current.text.find_first_of(".eE") != String::npos) {
                double value{};
                auto [end, ec] = std::from_chars(current.text.data(), current.text.data() + current.text.size(), value);
                if (ec != std::errc{} || end != current.text.data() + current.text.size() || !std::isfinite(value)) error("Invalid or out-of-range number");
                node->value = value;
            } else {
                UInt64 value{};
                auto [end, ec] = std::from_chars(current.text.data(), current.text.data() + current.text.size(), value);
                if (ec != std::errc{} || end != current.text.data() + current.text.size()) error("Invalid or out-of-range integer");
                node->value = value;
            }
            next(); return node;
        }
        if (current.kind == Token::Kind::identifier) {
            if (current.text == "true") node->value = true;
            else if (current.text == "false") node->value = false;
            else if (current.text != "null") { node->kind = Expression::Kind::variable; node->text = current.text; }
            next(); return node;
        }
        if (take("(")) { auto result = conditional(depth + 1); expect(")"); return result; }
        if (take("[")) {
            node->kind = Expression::Kind::array;
            if (!take("]")) {
                do { node->children.push_back(conditional(depth + 1)); } while (take(","));
                expect("]");
            }
            return seal(node);
        }
        if (take("{")) {
            node->kind = Expression::Kind::object;
            std::set<String> keys;
            if (!take("}")) {
                do {
                    if (token_.kind != Token::Kind::identifier && token_.kind != Token::Kind::string) error("Object keys must be names or strings");
                    if (!keys.insert(token_.text).second) error("Duplicate object key");
                    node->keys.push_back(token_.text); next(); expect(":"); node->children.push_back(conditional(depth + 1));
                } while (take(","));
                expect("}");
            }
            return seal(node);
        }
        error("Expected a Spear expression");
    }
    Syntax& syntax_;
    std::string_view source_;
    std::size_t start_, position_{0};
    Token token_;
};

class Parser {
public:
    explicit Parser(Template& file) : syntax_(file), source_(file.source) {}
    void parse() {
        syntax_.file.nodes = sequence({}, 0);
        bool extended = false;
        for (const auto& node : syntax_.file.nodes) {
            if (node.kind == Node::Kind::extends) {
                if (extended) fail(syntax_.file, node.location, "A Spear template can extend only one layout");
                extended = true;
            }
        }
        if (extended) for (const auto& node : syntax_.file.nodes) {
            if (node.kind == Node::Kind::extends || node.kind == Node::Kind::section) continue;
            if (node.kind == Node::Kind::text && std::all_of(node.text.begin(), node.text.end(), blank)) continue;
            fail(syntax_.file, node.location, "Extended templates must place output in sections");
        }
        // Account conservatively for source/owned strings, vector capacity and
        // shared AST nodes. A source-only cap can retain much larger parsed trees.
        syntax_.file.cache_cost = sizeof(Template) + 4 * syntax_.file.source.capacity() +
            2 * std::max(sizeof(Node), sizeof(Expression)) * syntax_.count;
    }
private:
    String directive() const {
        if (position_ == source_.size() || source_[position_] != '@') return {};
        auto end = position_ + 1;
        if (end == source_.size() || !word_start(source_[end])) return {};
        while (end < source_.size() && word_part(source_[end])) ++end;
        // Email domains such as user@if.example are ordinary HTML text.
        if (end < source_.size() && source_[end] == '.') return {};
        return String{source_.substr(position_ + 1, end - position_ - 1)};
    }
    std::pair<std::string_view, std::size_t> delimited(std::string_view close) {
        const auto start = position_;
        std::vector<char> delimiters;
        char quote = 0;
        while (position_ < source_.size()) {
            char c = source_[position_];
            if (quote) {
                if (c == '\\') { position_ += std::min<std::size_t>(2, source_.size() - position_); continue; }
                if (c == quote) quote = 0;
            } else {
                if (delimiters.empty() && source_.substr(position_).starts_with(close)) {
                    auto value = source_.substr(start, position_ - start); position_ += close.size(); return {value, start};
                }
                if (c == '\'' || c == '"') quote = c;
                else if (c == '(' || c == '[' || c == '{') {
                    delimiters.push_back(c);
                    if (delimiters.size() > 64) syntax_.error(position_, "Spear expression delimiter limit exceeded");
                } else if (c == ')' || c == ']' || c == '}') {
                    if (delimiters.empty() || delimiters.back() != (c == ')' ? '(' : c == ']' ? '[' : '{')) syntax_.error(position_, "Mismatched Spear expression delimiter");
                    delimiters.pop_back();
                }
            }
            ++position_;
        }
        syntax_.error(start, "Unclosed Spear expression or directive");
    }
    std::pair<std::string_view, std::size_t> header(const String& name) {
        position_ += name.size() + 1;
        while (position_ < source_.size() && blank(source_[position_])) ++position_;
        if (position_ == source_.size() || source_[position_] != '(') syntax_.error(position_, "@" + name + " requires parentheses");
        ++position_; return delimited(")");
    }
    std::vector<Expr> params(const String& name, std::size_t minimum, std::size_t maximum) {
        auto [value, start] = header(name);
        auto result = ExpressionParser{syntax_, value, start}.arguments();
        if (result.size() < minimum || result.size() > maximum) syntax_.error(start, "Wrong number of @" + name + " arguments");
        return result;
    }
    void close(const String& name) {
        if (directive() != name) syntax_.error(position_, "Expected @" + name);
        position_ += name.size() + 1;
        if (position_ < source_.size() && source_[position_] == '(') syntax_.error(position_, "@" + name + " does not accept arguments");
    }
    std::vector<Node> sequence(std::initializer_list<std::string_view> stops, unsigned depth) {
        if (depth > 64) syntax_.error(position_, "Spear template nesting limit exceeded");
        std::vector<Node> nodes;
        String text;
        auto text_start = position_;
        auto flush = [&] {
            if (text.empty()) return;
            syntax_.node(text_start, depth);
            Node node; node.location = syntax_.location(text_start); node.text = std::move(text); nodes.push_back(std::move(node)); text.clear();
        };
        auto push = [&](Node node) { flush(); nodes.push_back(std::move(node)); text_start = position_; };
        while (position_ < source_.size()) {
            auto start = position_;
            if (source_.substr(position_).starts_with("{{--")) {
                flush(); auto end = source_.find("--}}", position_ + 4);
                if (end == std::string_view::npos) syntax_.error(position_, "Unclosed Spear comment");
                position_ = end + 4; text_start = position_; continue;
            }
            if (source_.substr(position_).starts_with("@@")) { text += '@'; position_ += 2; continue; }
            if (source_.substr(position_).starts_with("@{{")) {
                auto end = source_.find("}}", position_ + 3);
                if (end == std::string_view::npos) syntax_.error(position_, "Unclosed escaped interpolation");
                text += source_.substr(position_ + 1, end + 2 - position_ - 1); position_ = end + 2; continue;
            }
            const bool raw = source_.substr(position_).starts_with("{!!");
            if (raw || source_.substr(position_).starts_with("{{")) {
                flush(); syntax_.node(start, depth);
                position_ += raw ? 3 : 2;
                auto [value, offset] = delimited(raw ? "!!}" : "}}");
                Node node; node.kind = raw ? Node::Kind::raw : Node::Kind::escaped; node.location = syntax_.location(start);
                node.expressions.push_back(ExpressionParser{syntax_, value, offset}.expression()); push(std::move(node)); continue;
            }
            const auto name = directive();
            if (!name.empty()) {
                if (std::find(stops.begin(), stops.end(), name) != stops.end()) { flush(); return nodes; }
                if (name == "else" || name == "elseif" || name == "empty" || name.starts_with("end")) syntax_.error(start, "Unexpected @" + name);
                if (name == "verbatim") {
                    position_ += name.size() + 1;
                    auto end = source_.find("@endverbatim", position_);
                    if (end == std::string_view::npos) syntax_.error(start, "Unclosed @verbatim block");
                    text += source_.substr(position_, end - position_); position_ = end + 12; continue;
                }
                if (name == "if" || name == "unless") {
                    flush(); syntax_.node(start, depth);
                    Node node; node.kind = Node::Kind::condition; node.location = syntax_.location(start);
                    auto args = params(name, 1, 1);
                    if (name == "unless") {
                        auto expression = std::make_shared<Expression>(); expression->kind = Expression::Kind::unary;
                        expression->text = "!"; expression->location = node.location; expression->children = args; args = {expression};
                    }
                    node.expressions = args;
                    node.branches.push_back(sequence({"elseif", "else", "endif", "endunless"}, depth + 1));
                    while (directive() == "elseif") {
                        auto branch = params("elseif", 1, 1); node.expressions.push_back(branch[0]);
                        node.branches.push_back(sequence({"elseif", "else", "endif", "endunless"}, depth + 1));
                    }
                    if (directive() == "else") {
                        close("else"); node.expressions.push_back({}); node.branches.push_back(sequence({"endif", "endunless"}, depth + 1));
                    }
                    close(name == "if" ? "endif" : "endunless"); push(std::move(node)); continue;
                }
                if (name == "foreach" || name == "forelse") {
                    flush(); syntax_.node(start, depth);
                    Node node; node.kind = Node::Kind::loop; node.location = syntax_.location(start);
                    auto [value, offset] = header(name); ExpressionParser{syntax_, value, offset}.loop(node);
                    node.children = sequence({"empty", "endforeach", "endforelse"}, depth + 1);
                    if (directive() == "empty" && name == "forelse") {
                        close("empty"); node.otherwise = sequence({"endforelse"}, depth + 1);
                    }
                    close(name == "foreach" ? "endforeach" : "endforelse"); push(std::move(node)); continue;
                }
                if (name == "include" || name == "yield" || name == "extends" || name == "section") {
                    flush(); syntax_.node(start, depth);
                    Node node; node.location = syntax_.location(start);
                    node.expressions = params(name, 1, name == "extends" ? 1 : 2);
                    if (name == "include") node.kind = Node::Kind::include;
                    else if (name == "yield") node.kind = Node::Kind::yield;
                    else if (name == "extends") {
                        if (depth != 0) syntax_.error(start, "@extends must be at template scope");
                        node.kind = Node::Kind::extends;
                    } else {
                        if (depth != 0) syntax_.error(start, "@section must be at template scope");
                        node.kind = Node::Kind::section;
                        if (node.expressions.size() == 1) { node.children = sequence({"endsection"}, depth + 1); close("endsection"); }
                    }
                    push(std::move(node)); continue;
                }
                auto after = position_ + name.size() + 1;
                while (after < source_.size() && blank(source_[after])) ++after;
                const bool css = name == "media" || name == "supports" || name == "container" ||
                                 name == "scope" || name == "layer" || name == "document";
                if (!css && after < source_.size() && source_[after] == '(')
                    syntax_.error(start, "Unknown Spear directive: @" + name);
                // Other @words remain HTML/CSS text. Native code is never evaluated.
            }
            text.push_back(source_[position_++]);
        }
        flush();
        if (stops.size() != 0) syntax_.error(position_, "Unclosed Spear directive block");
        return nodes;
    }
    Syntax syntax_;
    std::string_view source_;
    std::size_t position_{0};
};
} // namespace

std::shared_ptr<const Template> compile(String name, String source, bool spear) {
    if (source.size() > source_limit) throw SyntaxError{"View source limit exceeded", std::move(name), 1, 1};
    auto result = std::make_shared<Template>();
    result->name = std::move(name); result->source = std::move(source); result->spear = spear;
    result->cache_cost = sizeof(Template) + result->source.capacity();
    if (spear) Parser{*result}.parse();
    return result;
}
} // namespace gungnir::view::detail
