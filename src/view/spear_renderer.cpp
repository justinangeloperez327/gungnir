#include "spear.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <unordered_set>

namespace gungnir::view::detail {
namespace {
bool number(const Value& value) {
    return std::holds_alternative<Int64>(ValueAccess::storage(value)) || std::holds_alternative<UInt64>(ValueAccess::storage(value)) ||
           std::holds_alternative<Double>(ValueAccess::storage(value));
}
int order(UInt64 left, double right) {
    if (right < 0) return 1;
    if (right >= std::ldexp(1.0, 64)) return -1;
    auto integer = static_cast<UInt64>(right);
    if (left < integer) return -1;
    if (left > integer) return 1;
    return right == std::trunc(right) ? 0 : -1;
}
int order(Int64 left, double right) {
    if (right < -std::ldexp(1.0, 63)) return 1;
    if (right >= std::ldexp(1.0, 63)) return -1;
    auto integer = static_cast<Int64>(right);
    if (left < integer) return -1;
    if (left > integer) return 1;
    if (right == std::trunc(right)) return 0;
    return right < 0 ? 1 : -1;
}
int numeric_order(const Value& left, const Value& right) {
    return std::visit([](const auto& a, const auto& b) -> int {
        using A = std::remove_cvref_t<decltype(a)>;
        using B = std::remove_cvref_t<decltype(b)>;
        constexpr bool an = std::same_as<A, Int64> || std::same_as<A, UInt64> || std::same_as<A, Double>;
        constexpr bool bn = std::same_as<B, Int64> || std::same_as<B, UInt64> || std::same_as<B, Double>;
        if constexpr (an && bn) {
            if constexpr (std::same_as<A, Double> && std::same_as<B, Double>) return a < b ? -1 : a > b ? 1 : 0;
            else if constexpr (std::same_as<A, Double>) return -order(b, a);
            else if constexpr (std::same_as<B, Double>) return order(a, b);
            else return std::cmp_less(a, b) ? -1 : std::cmp_greater(a, b) ? 1 : 0;
        } else return 0;
    }, ValueAccess::storage(left), ValueAccess::storage(right));
}
bool finite(const Value& value) {
    auto floating = std::get_if<Double>(&ValueAccess::storage(value));
    return !floating || std::isfinite(*floating);
}
bool equal(const Value& left, const Value& right, Session& session, unsigned depth = 0) {
    session.step();
    if (depth > 64) throw SyntaxError{"Spear value comparison nesting limit exceeded"};
    if (number(left) && number(right)) return finite(left) && finite(right) && numeric_order(left, right) == 0;
    if (ValueAccess::storage(left).index() != ValueAccess::storage(right).index()) return false;
    if (left.is_null()) return true;
    if (left.is_array()) {
        auto& a = left.as_array(); auto& b = right.as_array();
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) if (!equal(a[i], b[i], session, depth + 1)) return false;
        return true;
    }
    if (left.is_object()) {
        auto& a = left.as_object(); auto& b = right.as_object();
        if (a.size() != b.size()) return false;
        for (const auto& [key, value] : a) {
            auto found = b.find(key);
            if (found == b.end() || !equal(value, found->second, session, depth + 1)) return false;
        }
        return true;
    }
    return left.string() == right.string();
}
std::optional<std::size_t> array_index(const Value& value) {
    if (auto integer = std::get_if<Int64>(&ValueAccess::storage(value))) {
        if (*integer >= 0 && std::cmp_less_equal(*integer, std::numeric_limits<std::size_t>::max())) return static_cast<std::size_t>(*integer);
    } else if (auto unsigned_integer = std::get_if<UInt64>(&ValueAccess::storage(value))) {
        if (std::cmp_less_equal(*unsigned_integer, std::numeric_limits<std::size_t>::max())) return static_cast<std::size_t>(*unsigned_integer);
    }
    return {};
}
} // namespace

String escape(std::string_view source) {
    if (source.size() > output_limit) throw SyntaxError{"View output limit exceeded"};
    String result;
    result.reserve(source.size());
    for (const char c : source) {
        std::string_view encoded;
        switch (c) {
        case '&': encoded = "&amp;"; break;
        case '<': encoded = "&lt;"; break;
        case '>': encoded = "&gt;"; break;
        case '\'': encoded = "&#39;"; break;
        case '"': encoded = "&quot;"; break;
        default: result.push_back(c); break;
        }
        if (!encoded.empty()) result += encoded;
        if (result.size() > output_limit) throw SyntaxError{"View output limit exceeded"};
    }
    return result;
}
void Session::append(String& output, std::string_view value) {
    if (value.size() > remaining_) throw SyntaxError{"View output limit exceeded"};
    remaining_ -= value.size(); output += value;
}
void Session::step() {
    if (operations_ == 0) throw SyntaxError{"View operation limit exceeded"};
    --operations_;
}
String Session::render(std::string_view name, const Data& data, const Value* current, Sections& sections, unsigned depth) {
    if (depth > 64) throw SyntaxError{"View composition nesting limit exceeded"};
    step();
    const auto file = engine.load(root_, name);
    if (std::find(stack_.begin(), stack_.end(), file->name) != stack_.end())
        throw SyntaxError{"Cyclic view composition", file->name, 1, 1};
    stack_.push_back(file->name);
    struct Pop { std::vector<String>& stack; ~Pop() { stack.pop_back(); } } pop{stack_};
    if (!file->spear) return render_legacy(file->source, data, current, *this, sections, depth);
    if (current) {
        auto adapted = data;
        if (current->is_object()) for (const auto& [key, value] : current->as_object()) adapted.with(key, value);
        adapted.with("this", *current);
        return spear(*file, adapted, sections, depth);
    }
    return spear(*file, data, sections, depth);
}

Value Session::evaluate(const Template& file, const Expr& expression, const Data& data, unsigned depth) {
    step();
    const auto& node = *expression;
    if (depth > 64) fail(file, node.location, "Spear expression evaluation nesting limit exceeded");
    auto child = [&](std::size_t index) { return evaluate(file, node.children[index], data, depth + 1); };
    switch (node.kind) {
    case Expression::Kind::literal: return node.value;
    case Expression::Kind::variable: {
        auto value = data.get(node.text); return value ? *value : Value{};
    }
    case Expression::Kind::member: {
        auto object = child(0); auto value = object.get(node.text); return value ? *value : Value{};
    }
    case Expression::Kind::index: {
        auto object = child(0); auto key = child(1);
        if (object.is_array()) {
            auto index = array_index(key);
            if (index && *index < object.as_array().size()) return object.as_array()[*index];
        } else if (object.is_object() && std::holds_alternative<String>(ValueAccess::storage(key))) {
            if (auto value = object.get(key.string())) return *value;
        }
        return nullptr;
    }
    case Expression::Kind::array: {
        std::vector<Value> values;
        for (std::size_t i = 0; i < node.children.size(); ++i) values.push_back(child(i));
        return Value::array(std::move(values));
    }
    case Expression::Kind::object: {
        std::unordered_map<String, Value> values;
        for (std::size_t i = 0; i < node.children.size(); ++i) values.emplace(node.keys[i], child(i));
        return Value::object(std::move(values));
    }
    case Expression::Kind::helper: {
        std::vector<Value> arguments;
        for (std::size_t i = 0; i < node.children.size(); ++i) arguments.push_back(child(i));
        try { return engine.call(node.text, arguments); }
        catch (const SyntaxError& error) {
            if (!error.template_name().empty()) throw;
            fail(file, node.location, error.what());
        } catch (const std::exception&) { fail(file, node.location, "View helper failed: " + node.text); }
    }
    case Expression::Kind::unary: {
        auto value = child(0);
        if (node.text == "!") return !value.truthy();
        if (auto integer = std::get_if<Int64>(&ValueAccess::storage(value))) {
            if (*integer == std::numeric_limits<Int64>::min()) fail(file, node.location, "Integer negation overflow");
            return -*integer;
        }
        if (auto integer = std::get_if<UInt64>(&ValueAccess::storage(value))) {
            constexpr auto minimum_magnitude = UInt64{1} << 63;
            if (*integer > minimum_magnitude) fail(file, node.location, "Integer negation overflow");
            if (*integer == minimum_magnitude) return std::numeric_limits<Int64>::min();
            return -static_cast<Int64>(*integer);
        }
        if (auto floating = std::get_if<Double>(&ValueAccess::storage(value)); floating && std::isfinite(*floating)) return -*floating;
        fail(file, node.location, "Unary '-' requires a finite number");
    }
    case Expression::Kind::binary: {
        auto left = child(0);
        if (node.text == "&&") return left.truthy() && child(1).truthy();
        if (node.text == "||") return left.truthy() || child(1).truthy();
        if (node.text == "??") return left.is_null() ? child(1) : left;
        auto right = child(1);
        if (node.text == "==" || node.text == "===" || node.text == "!=" || node.text == "!==") {
            auto same = equal(left, right, *this);
            return (node.text == "!=" || node.text == "!==") ? !same : same;
        }
        int comparison;
        if (number(left) && number(right) && finite(left) && finite(right)) comparison = numeric_order(left, right);
        else if (auto a = std::get_if<String>(&ValueAccess::storage(left))) {
            if (auto b = std::get_if<String>(&ValueAccess::storage(right))) comparison = a->compare(*b);
            else fail(file, node.location, "Comparison requires two numbers or two strings");
        } else fail(file, node.location, "Comparison requires two finite numbers or two strings");
        if (node.text == "<") return comparison < 0;
        if (node.text == ">") return comparison > 0;
        if (node.text == "<=") return comparison <= 0;
        return comparison >= 0;
    }
    case Expression::Kind::conditional: return child(0).truthy() ? child(1) : child(2);
    }
    fail(file, node.location, "Invalid Spear expression node");
}

String Session::nodes(const Template& file, const std::vector<Node>& source, const Data& data, Sections& sections, unsigned depth) {
    if (depth > 64) throw SyntaxError{"View composition nesting limit exceeded", file.name, 1, 1};
    String output;
    std::unordered_set<String> defined_sections;
    for (const auto& node : source) {
        step();
        auto value = [&](std::size_t index) { return evaluate(file, node.expressions[index], data); };
        try {
            switch (node.kind) {
            case Node::Kind::text: append(output, node.text); break;
            case Node::Kind::escaped: append(output, escape(value(0).string())); break;
            case Node::Kind::raw: append(output, value(0).string()); break;
            case Node::Kind::extends: break;
            case Node::Kind::condition:
                for (std::size_t i = 0; i < node.branches.size(); ++i) {
                    if (!node.expressions[i] || value(i).truthy()) {
                        append(output, nodes(file, node.branches[i], data, sections, depth + 1)); break;
                    }
                }
                break;
            case Node::Kind::loop: {
                auto collection = value(0);
                if (!collection.is_null() && !collection.is_array() && !collection.is_object()) fail(file, node.location, "Spear loops require an array, object, or null");
                const auto count = collection.is_array() ? collection.as_array().size() : collection.is_object() ? collection.as_object().size() : 0;
                if (count > 1000000) fail(file, node.location, "View iteration limit exceeded");
                if (count == 0) { append(output, nodes(file, node.otherwise, data, sections, depth + 1)); break; }
                std::vector<String> keys;
                if (collection.is_object()) {
                    for (const auto& [key, unused] : collection.as_object()) keys.push_back(key);
                    std::sort(keys.begin(), keys.end());
                }
                const auto* parent = data.get("loop");
                UInt64 loop_depth = 1;
                if (parent) if (auto parent_depth = parent->get("depth")) {
                    if (auto integer = std::get_if<UInt64>(&ValueAccess::storage(*parent_depth))) loop_depth = *integer + 1;
                }
                for (std::size_t i = 0; i < count; ++i) {
                    step(); auto child = data;
                    const auto& item = collection.is_array() ? collection.as_array()[i] : collection.as_object().at(keys[i]);
                    child.with(node.variable, item).with("this", item);
                    if (!node.key.empty()) child.with(node.key, collection.is_array() ? Value{static_cast<UInt64>(i)} : Value{keys[i]});
                    child.with("loop", Value::object({
                        {"index", Value{static_cast<UInt64>(i)}}, {"iteration", Value{static_cast<UInt64>(i + 1)}},
                        {"remaining", Value{static_cast<UInt64>(count - i - 1)}}, {"count", Value{static_cast<UInt64>(count)}},
                        {"first", Value{i == 0}}, {"last", Value{i + 1 == count}}, {"depth", Value{loop_depth}},
                        {"parent", parent ? *parent : Value{}}
                    }));
                    append(output, nodes(file, node.children, child, sections, depth + 1));
                }
                break;
            }
            case Node::Kind::include: {
                auto child = data;
                if (node.expressions.size() == 2) {
                    auto props = value(1);
                    if (!props.is_object()) fail(file, node.location, "@include data must be an object");
                    for (const auto& [key, entry] : props.as_object()) child.with(key, entry);
                }
                auto scope = sections;
                append(output, render(value(0).string(), child, nullptr, scope, depth + 1)); break;
            }
            case Node::Kind::section: {
                auto name = value(0).string();
                if (name.empty()) fail(file, node.location, "Section name cannot be empty");
                if (!defined_sections.insert(name).second) fail(file, node.location, "Duplicate section definition: " + name);
                if (!sections.contains(name)) {
                    auto body = node.expressions.size() == 2 ? escape(value(1).string()) : nodes(file, node.children, data, sections, depth + 1);
                    sections.emplace(std::move(name), std::move(body));
                }
                break;
            }
            case Node::Kind::yield: {
                auto found = sections.find(value(0).string());
                if (found != sections.end()) append(output, found->second);
                else if (node.expressions.size() == 2) append(output, escape(value(1).string()));
                break;
            }
            }
        } catch (const SyntaxError& error) {
            if (!error.template_name().empty()) throw;
            fail(file, node.location, error.what());
        } catch (const NotFound&) { fail(file, node.location, "Included template or layout was not found"); }
        catch (const InvalidPath&) { fail(file, node.location, "Included template or layout path is invalid"); }
    }
    return output;
}

String Session::spear(const Template& file, const Data& data, Sections& sections, unsigned depth) {
    auto result = nodes(file, file.nodes, data, sections, depth);
    for (const auto& node : file.nodes) if (node.kind == Node::Kind::extends) {
        try { return render(evaluate(file, node.expressions[0], data).string(), data, nullptr, sections, depth + 1); }
        catch (const SyntaxError& error) {
            if (!error.template_name().empty()) throw;
            fail(file, node.location, error.what());
        } catch (const NotFound&) { fail(file, node.location, "Layout template was not found"); }
        catch (const InvalidPath&) { fail(file, node.location, "Layout path is invalid"); }
    }
    return result;
}
} // namespace gungnir::view::detail
