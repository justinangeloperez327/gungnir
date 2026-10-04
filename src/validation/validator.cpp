#include <gungnir/validation/validator.hpp>
#include <gungnir/validation/definition.hpp>
#include <gungnir/validation/exception.hpp>
#include <gungnir/database/runtime.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <mutex>
#include <unordered_set>

namespace gungnir::validation {
namespace {
using Json = http::Json;
using View = std::string_view;
bool has(const std::vector<ParsedRule>& rules, View name) {
    return std::any_of(rules.begin(), rules.end(), [&](const auto& rule) { return rule.name == name; });
}
bool numeric(View text) {
    double value{}; const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return !text.empty() && error == std::errc{} && end == text.data() + text.size() && std::isfinite(value);
}
bool integer(View text) {
    if (text.empty()) return false;
    if (text.front() == '-') {
        Int64 value{}; const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        return error == std::errc{} && end == text.data() + text.size();
    }
    UInt64 value{}; const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}
// Compare decimal representations without rounding UInt64 bounds through a
// double. This is also exact when MSVC's long double has double precision.
struct Decimal { bool negative{}; Int64 magnitude{}; String digits; };
Decimal decimal(View value) {
    Decimal result;
    if (value.starts_with('-')) { result.negative = true; value.remove_prefix(1); }
    const auto e = value.find_first_of("eE");
    auto power = e == View::npos ? View{} : value.substr(e + 1);
    if (e != View::npos) value = value.substr(0, e);
    const auto dot = value.find('.');
    for (char c : value) if (c != '.') result.digits += c;
    const auto first = result.digits.find_first_not_of('0'); if (first == String::npos) return {};
    int exponent{};
    if (power.starts_with('+')) power.remove_prefix(1);
    if (!power.empty()) (void)std::from_chars(power.data(), power.data() + power.size(), exponent);
    result.magnitude = static_cast<Int64>(dot == View::npos ? value.size() : dot) + exponent - static_cast<Int64>(first);
    result.digits.erase(0, first);
    while (result.digits.ends_with('0')) result.digits.pop_back();
    return result;
}
int compare(View left, View right) {
    const auto a = decimal(left), b = decimal(right);
    if (a.digits.empty() && b.digits.empty()) return 0;
    if (a.negative != b.negative) return a.negative ? -1 : 1;
    int order{};
    if (a.digits.empty()) order = -1;
    else if (b.digits.empty()) order = 1;
    else if (a.magnitude != b.magnitude) order = a.magnitude < b.magnitude ? -1 : 1;
    else for (std::size_t i = 0; i < std::max(a.digits.size(), b.digits.size()); ++i) {
        const char x = i < a.digits.size() ? a.digits[i] : '0', y = i < b.digits.size() ? b.digits[i] : '0';
        if (x != y) { order = x < y ? -1 : 1; break; }
    }
    return a.negative ? -order : order;
}
bool equal(const Json& a, const Json& b) {
    if (a.is_number() && b.is_number()) return numeric(a.string()) && numeric(b.string()) && compare(a.string(), b.string()) == 0;
    return a == b;
}
bool boolean(View text) { return text == "true" || text == "false" || text == "1" || text == "0" || text == "yes" || text == "no" || text == "on" || text == "off"; }
bool accepted(View text) { return text == "true" || text == "1" || text == "yes" || text == "on"; }
bool domain(View host) {
    if (host.empty() || host.size() > 253) return false;
    while (true) {
        const auto dot = host.find('.'); const auto label = host.substr(0, dot);
        if (label.empty() || label.size() > 63 || label.starts_with('-') || label.ends_with('-') || !std::all_of(label.begin(), label.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'; })) return false;
        if (dot == View::npos) return true;
        host.remove_prefix(dot + 1);
    }
}
bool email(View text) {
    if (text.size() > 254 || std::any_of(text.begin(), text.end(), [](unsigned char c) { return c < 33 || c >= 127; })) return false;
    const auto at = text.find('@');
    if (at == 0 || at == View::npos || at > 64 || text.find('@', at + 1) != View::npos) return false;
    const auto local = text.substr(0, at), host = text.substr(at + 1);
    if (local.starts_with('.') || local.ends_with('.') || local.find("..") != View::npos || !std::all_of(local.begin(), local.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || View{".!#$%&'*+-/=?^_`{|}~"}.find(c) != View::npos; })) return false;
    return host.find('.') != View::npos && domain(host);
}
bool hex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
bool uuid(View text) {
    if (text.size() != 36) return false;
    for (std::size_t i = 0; i < text.size(); ++i) if (i == 8 || i == 13 || i == 18 || i == 23 ? text[i] != '-' : !hex(text[i])) return false;
    return true;
}
bool ipv4(View host) {
    int pieces{};
    while (true) {
        const auto dot = host.find('.'); const auto token = host.substr(0, dot); unsigned value{};
        const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
        if (token.empty() || (token.size() > 1 && token.front() == '0') || error != std::errc{} || end != token.data() + token.size() || value > 255) return false;
        ++pieces; if (dot == View::npos) return pieces == 4; host.remove_prefix(dot + 1);
    }
}
bool ipv6(View host) {
    const auto compressed = host.find("::"); if (compressed != View::npos && host.find("::", compressed + 2) != View::npos) return false;
    auto count = [](View part) -> int {
        if (part.empty()) return 0;
        int groups{};
        while (true) {
            const auto colon = part.find(':'); const auto token = part.substr(0, colon);
            if (token.find('.') != View::npos) return colon == View::npos && ipv4(token) ? groups + 2 : -1;
            if (token.empty() || token.size() > 4 || !std::all_of(token.begin(), token.end(), hex)) return -1;
            ++groups; if (colon == View::npos) return groups; part.remove_prefix(colon + 1);
        }
    };
    if (compressed == View::npos) return count(host) == 8;
    const auto before = host.substr(0, compressed), after = host.substr(compressed + 2);
    if (before.find('.') != View::npos) return false;
    const int left = count(before), right = count(after); return left >= 0 && right >= 0 && left + right < 8;
}
bool url(View text) {
    String scheme{text.substr(0, text.find("://"))}; for (auto& c : scheme) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    if ((scheme != "http" && scheme != "https") || text.substr(scheme.size(), 3) != "://") return false;
    text.remove_prefix(scheme.size() + 3); const auto suffix = text.find_first_of("/?#"); const auto authority = text.substr(0, suffix);
    if (authority.empty() || authority.find('@') != View::npos) return false;
    View port;
    if (authority.starts_with('[')) {
        const auto close = authority.find(']'); if (close == View::npos || !ipv6(authority.substr(1, close - 1))) return false;
        if (close + 1 < authority.size()) { if (authority[close + 1] != ':') return false; port = authority.substr(close + 2); if (port.empty()) return false; }
    } else {
        const auto colon = authority.find(':'); const auto host = authority.substr(0, colon);
        if (host.empty()) return false;
        if (std::all_of(host.begin(), host.end(), [](char c) { return (c >= '0' && c <= '9') || c == '.'; })) { if (!ipv4(host)) return false; } else if (!domain(host)) return false;
        if (colon != View::npos) { port = authority.substr(colon + 1); if (port.empty()) return false; }
    }
    if (!port.empty()) { unsigned value{}; const auto [end, error] = std::from_chars(port.data(), port.data() + port.size(), value); if (error != std::errc{} || end != port.data() + port.size() || value > 65535) return false; }
    const auto tail = suffix == View::npos ? View{} : text.substr(suffix); bool fragment{};
    for (std::size_t i = 0; i < tail.size(); ++i) {
        const char c = tail[i];
        if (c == '#') { if (fragment) return false; fragment = true; }
        else if (c == '%') { if (i + 2 >= tail.size() || !hex(tail[i + 1]) || !hex(tail[i + 2])) return false; i += 2; }
        else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || View{"-._~!$&'()*+,;=:@/?"}.find(c) != View::npos)) return false;
    }
    return true;
}
int digits(View text) {
    if (text.empty() || !std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; })) return -1;
    int value{}; (void)std::from_chars(text.data(), text.data() + text.size(), value); return value;
}
bool date(View text) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') return false;
    const int year = digits(text.substr(0, 4)), month = digits(text.substr(5, 2)), day = digits(text.substr(8, 2));
    if (year < 1 || month < 1 || month > 12 || day < 1) return false;
    constexpr int days[]{31,28,31,30,31,30,31,31,30,31,30,31}; return day <= days[month - 1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}
bool datetime(View text) {
    if (text.size() < 20 || !date(text.substr(0, 10)) || (text[10] != 'T' && text[10] != 't') || text[13] != ':' || text[16] != ':') return false;
    const int hour = digits(text.substr(11, 2)), minute = digits(text.substr(14, 2)), second = digits(text.substr(17, 2));
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return false;
    auto rest = text.substr(19);
    if (rest.starts_with('.')) { rest.remove_prefix(1); std::size_t count{}; while (count < rest.size() && rest[count] >= '0' && rest[count] <= '9') ++count; if (!count) return false; rest.remove_prefix(count); }
    if (rest == "Z" || rest == "z") return true;
    if (rest.size() != 6 || (rest[0] != '+' && rest[0] != '-') || rest[3] != ':') return false;
    const int hour_offset = digits(rest.substr(1, 2)), minute_offset = digits(rest.substr(4, 2)); return hour_offset >= 0 && hour_offset <= 23 && minute_offset >= 0 && minute_offset <= 59;
}
const Json* at_path(const Json& input, View path) {
    const auto dot = path.find('.'); const auto key = path.substr(0, dot); const Json* next{};
    if (input.is_object()) next = input.get(key);
    else if (input.is_array()) { std::size_t index{}; const auto [end, error] = std::from_chars(key.data(), key.data() + key.size(), index); if (!key.empty() && error == std::errc{} && end == key.data() + key.size() && index < input.as_array().size()) next = &input.as_array()[index]; }
    return next && dot != View::npos ? at_path(*next, path.substr(dot + 1)) : next;
}
Json select_path(Json output, const Json& input, View path, const Json& value) {
    const auto dot = path.find('.'); const auto key = path.substr(0, dot);
    if (input.is_array()) {
        auto array = output.is_array() ? output.as_array() : Json::Array{}; std::size_t index{}; (void)std::from_chars(key.data(), key.data() + key.size(), index);
        if (index >= input.as_array().size()) return output;
        if (array.size() <= index) array.resize(index + 1);
        array[index] = dot == View::npos ? value : select_path(array[index], input.as_array()[index], path.substr(dot + 1), value); return Json::array(std::move(array));
    }
    auto object = output.is_object() ? output.as_object() : Json::Object{}; const auto* child = input.get(key);
    object[String{key}] = dot == View::npos ? value : select_path(object[String{key}], child ? *child : Json{}, path.substr(dot + 1), value); return Json::object(std::move(object));
}
void expand(const Json& input, View pattern, String prefix, std::vector<String>& paths) {
    const auto star = pattern.find('*'); if (star == View::npos) { paths.push_back(prefix + String{pattern}); return; }
    const auto parent_path = prefix + String{pattern.substr(0, star ? star - 1 : 0)}; const auto* parent = parent_path.empty() ? &input : at_path(input, parent_path);
    if (!parent || !parent->is_array()) return;
    for (std::size_t i = 0; i < parent->as_array().size(); ++i) expand(input, pattern.substr(star + 1), (parent_path.empty() ? "" : parent_path + ".") + std::to_string(i), paths);
}
String reference(View argument, View pattern, View path) {
    String result;
    while (true) {
        const auto dot = argument.find('.'), pdot = pattern.find('.'), cdot = path.find('.'); const auto part = argument.substr(0, dot);
        if (!result.empty()) result += '.';
        result += part == "*" && pattern.substr(0, pdot) == "*" ? path.substr(0, cdot) : part;
        if (dot == View::npos) return result;
        argument.remove_prefix(dot + 1);
        pattern = pdot == View::npos ? View{} : pattern.substr(pdot + 1); path = cdot == View::npos ? View{} : path.substr(cdot + 1);
    }
}
bool descendant(View pattern, View path) {
    while (true) {
        const auto dot = pattern.find('.'), pdot = path.find('.');
        if (pattern.substr(0, dot) != "*" && pattern.substr(0, dot) != path.substr(0, pdot)) return false;
        if (pdot == View::npos) return dot != View::npos;
        if (dot == View::npos) return false;
        pattern.remove_prefix(dot + 1); path.remove_prefix(pdot + 1);
    }
}
View upload_key(View name) { return name.ends_with("[]") ? name.substr(0, name.size() - 2) : name; }
String message(const String& field, const ParsedRule& rule) {
    const auto& name = rule.name;
    if (name == "required" || name == "requiredIf") return "The " + field + " field is required.";
    if (name == "present") return "The " + field + " field must be present.";
    if (name == "integer") return "The " + field + " field must be an integer.";
    if (name == "numeric") return "The " + field + " field must be numeric.";
    if (name == "boolean") return "The " + field + " field must be true or false.";
    if (name == "email") return "The " + field + " field must be a valid email address.";
    if (name == "min") return "The " + field + " field must be at least " + rule.arguments[0] + ".";
    if (name == "max") return "The " + field + " field must not be greater than " + rule.arguments[0] + ".";
    if (name == "length" || name == "size") return "The " + field + " field must have size " + rule.arguments[0] + ".";
    if (name == "same") return "The " + field + " field must match " + rule.arguments[0] + ".";
    if (name == "different") return "The " + field + " field must differ from " + rule.arguments[0] + ".";
    if (name == "confirmed") return "The " + field + " field confirmation does not match.";
    if (name == "accepted") return "The " + field + " field must be accepted.";
    return "The " + field + " field failed the " + name + " rule.";
}
bool database_rule(const Json& value, const ParsedRule& rule, View field) {
    if (value.is_array() || value.is_object() || value.is_null()) return false;
    const auto connection = database::runtime::read_connection(); const auto backend = connection->backend();
    if (backend == database::Backend::mongodb) throw std::logic_error("Database validation requires a SQL connection");
    auto identifier = [backend](View name) { return backend == database::Backend::mysql ? "`" + String{name} + "`" : "\"" + String{name} + "\""; };
    auto placeholder = [backend](int index) { return backend == database::Backend::postgresql ? "$" + std::to_string(index) : "?"; };
    const auto& args = rule.arguments; String sql = "SELECT COUNT(*) AS matches FROM " + identifier(args[0]) + " WHERE " + identifier(args.size() > 1 ? View{args[1]} : field) + " = " + placeholder(1);
    auto attribute = [](const Json& data) -> model::AttributeValue {
        if (data.is_string()) return data.string();
        if (data.is_boolean()) return data.string() == "true";
        const auto text = data.string();
        if (data.is_integer()) {
            if (text.starts_with('-')) { Int64 number{}; (void)std::from_chars(text.data(), text.data() + text.size(), number); return number; }
            UInt64 number{}; (void)std::from_chars(text.data(), text.data() + text.size(), number); return number;
        }
        double number{}; (void)std::from_chars(text.data(), text.data() + text.size(), number); return number;
    };
    std::vector<model::AttributeValue> bindings{attribute(value)};
    if (args.size() > 2) { sql += " AND " + identifier(args.size() > 3 ? View{args[3]} : View{"id"}) + " <> " + placeholder(2); bindings.emplace_back(args[2]); }
    const auto result = connection->execute(sql, bindings);
    if (result.rows.empty() || !result.rows.front().contains("matches")) throw std::runtime_error("Database validation returned no count");
    const auto count = model::value_cast<UInt64>(result.rows.front().at("matches")); return rule.name == "exists" ? count != 0 : count == 0;
}
StructuredResult check_data(const Json& source, const Rules& definitions, Context context, bool flat = false) {
    if (!source.is_object()) throw std::invalid_argument("Structured validation requires an object");
    std::vector<std::vector<ParsedRule>> expressions; std::unordered_map<String, Engine::CustomRule> custom; std::unordered_set<String> fields;
    for (const auto& entry : definitions.entries()) {
        if (!fields.insert(entry.field).second) throw std::logic_error("Duplicate validation field path");
        auto parsed = parse_rule_expression(entry.field, entry.expression);
        for (const auto& rule : parsed) if (rule.name == "custom" && !custom.contains(rule.arguments[0])) {
            if (!context.custom_rules) throw std::logic_error("Custom validation requires a rule registry");
            custom.emplace(rule.arguments[0], context.custom_rules->rule(rule.arguments[0]));
        }
        expressions.push_back(std::move(parsed));
    }
    auto input = source; std::unordered_map<String, const http::UploadedFile*> uploads;
    if (context.uploads) {
        std::unordered_map<String, std::vector<const http::UploadedFile*>> groups;
        for (const auto& file : *context.uploads) groups[String{upload_key(file.name)}].push_back(&file);
        auto values = input.as_object();
        for (const auto& [name, files] : groups) {
            const bool multiple = files.size() > 1 || files.front()->name.ends_with("[]"); Json::Array metadata;
            for (std::size_t i = 0; i < files.size(); ++i) {
                const auto* file = files[i]; metadata.push_back(Json::object({{"name", Json{file->filename}}, {"contentType", Json{file->content_type}}, {"size", Json{static_cast<UInt64>(file->size())}}}));
                uploads.emplace(multiple ? name + "." + std::to_string(i) : name, file);
            }
            values.insert_or_assign(name, multiple ? Json::array(std::move(metadata)) : metadata.front());
        }
        input = Json::object(std::move(values));
    }
    StructuredResult result{Json::object({}), {}}; std::vector<std::pair<String, Json>> selected;
    auto lookup = [&](View path) { return flat ? input.get(path) : at_path(input, path); };
    for (std::size_t entry_index = 0; entry_index < definitions.size(); ++entry_index) {
        const auto& entry = definitions.entries()[entry_index]; const auto& parsed = expressions[entry_index]; std::vector<String> paths;
        if (flat) paths.push_back(entry.field); else expand(input, entry.field, "", paths);
        for (const auto& path : paths) {
            const auto* value = lookup(path); if (!value && has(parsed, "sometimes")) continue;
            bool required = has(parsed, "required");
            for (const auto& rule : parsed) if (rule.name == "requiredIf") { const auto* other = lookup(reference(rule.arguments[0], entry.field, path)); if (other && !other->is_array() && !other->is_object() && !other->is_null()) required = required || std::find(rule.arguments.begin() + 1, rule.arguments.end(), other->string()) != rule.arguments.end(); }
            const bool empty = !value || value->is_null() || (value->is_string() && rule_trim(value->string()).empty()) || (value->is_array() && value->as_array().empty()) || (value->is_object() && value->as_object().empty());
            if (required && empty) { result.errors[path].push_back(message(path, {"required", {}})); continue; }
            if (!value) { if (has(parsed, "present")) result.errors[path].push_back(message(path, {"present", {}})); continue; }
            if (has(parsed, "nullable") && (value->is_null() || (flat && empty))) { selected.emplace_back(path, *value); continue; }
            const auto upload = uploads.find(path); const auto* file = upload == uploads.end() ? nullptr : upload->second;
            const bool scalar = !value->is_array() && !value->is_object() && !value->is_null(); const auto text = scalar ? value->string() : String{}; bool valid = true;
            for (const auto& rule : parsed) {
                const auto& name = rule.name;
                if (name == "required" || name == "requiredIf" || name == "present" || name == "nullable" || name == "sometimes" || name == "bail") continue;
                bool pass = false;
                if (name == "string") pass = !file && value->is_string();
                else if (name == "array") pass = !file && value->is_array();
                else if (name == "object") pass = !file && value->is_object();
                else if (name == "integer") pass = !file && (value->is_integer() || (value->is_string() && integer(text)));
                else if (name == "numeric") pass = !file && (value->is_number() || value->is_string()) && numeric(text);
                else if (name == "boolean") pass = !file && (value->is_boolean() || value->is_integer() || value->is_string()) && boolean(text);
                else if (name == "accepted") pass = !file && (value->is_string() || value->is_integer() || value->is_boolean()) && accepted(text);
                else if (name == "email") pass = !file && value->is_string() && email(text);
                else if (name == "uuid") pass = !file && value->is_string() && uuid(text);
                else if (name == "url") pass = !file && value->is_string() && url(text);
                else if (name == "date") pass = !file && value->is_string() && date(text);
                else if (name == "datetime") pass = !file && value->is_string() && datetime(text);
                else if (name == "same" || name == "different" || name == "confirmed") { const auto* other = lookup(name == "confirmed" ? path + "_confirmation" : reference(rule.arguments[0], entry.field, path)); pass = !file && other && (name == "different" ? !equal(*value, *other) : equal(*value, *other)); }
                else if (name == "in" || name == "notIn") { const bool found = scalar && std::find(rule.arguments.begin(), rule.arguments.end(), text) != rule.arguments.end(); pass = !file && scalar && (name == "in" ? found : !found); }
                else if (name == "min" || name == "max" || name == "size" || name == "length") {
                    String measure;
                    if (file) measure = std::to_string(file->size()); else if (value->is_array()) measure = std::to_string(value->as_array().size()); else if (value->is_object()) measure = std::to_string(value->as_object().size());
                    else if ((has(parsed, "numeric") || has(parsed, "integer") || value->is_number()) && name != "length") { if (scalar && numeric(text)) measure = text; } else if (value->is_string()) measure = std::to_string(text.size());
                    if (!measure.empty()) {
                        auto order = compare(measure, rule.arguments[0]);
                        if (!file && value->is_number() && !value->is_integer()) {
                            double number{}, bound{};
                            (void)std::from_chars(measure.data(), measure.data() + measure.size(), number);
                            (void)std::from_chars(rule.arguments[0].data(), rule.arguments[0].data() + rule.arguments[0].size(), bound);
                            order = number < bound ? -1 : number > bound ? 1 : 0;
                        }
                        pass = name == "min" ? order >= 0 : name == "max" ? order <= 0 : order == 0;
                    }
                } else if (name == "file") pass = file != nullptr;
                else if (name == "image") pass = file && http::detected_media_type(file->bytes()).starts_with("image/");
                else if (name == "mimes" || name == "mimetypes") {
                    if (file) { const auto detected = http::detected_media_type(file->bytes()); for (const auto& argument : rule.arguments) { const auto media = name == "mimetypes" ? argument.c_str() : argument == "png" ? "image/png" : argument == "jpg" || argument == "jpeg" ? "image/jpeg" : argument == "gif" ? "image/gif" : argument == "webp" ? "image/webp" : argument == "pdf" ? "application/pdf" : argument == "txt" ? "text/plain" : "application/octet-stream"; pass = pass || detected == media; } }
                } else if (name == "extensions") {
                    if (file) { const auto dot = file->filename.find_last_of('.'); auto extension = dot == String::npos ? String{} : file->filename.substr(dot + 1); for (auto& c : extension) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32); pass = std::find(rule.arguments.begin(), rule.arguments.end(), extension) != rule.arguments.end(); }
                } else if (name == "custom") pass = custom.at(rule.arguments[0]).predicate(*value, input);
                else if (name == "unique" || name == "exists") pass = !file && database_rule(*value, rule, entry.field);
                if (!pass) { valid = false; result.errors[path].push_back(name == "custom" ? custom.at(rule.arguments[0]).message : message(path, rule)); if (has(parsed, "bail")) break; }
            }
            if (valid) {
                const bool children = !flat && !file && (value->is_object() || value->is_array()) && std::any_of(definitions.entries().begin(), definitions.entries().end(), [&](const auto& definition) { return descendant(definition.field, path); });
                selected.emplace_back(path, children ? (value->is_array() ? Json::array(Json::Array(value->as_array().size())) : Json::object({})) : *value);
            }
        }
    }
    std::stable_sort(selected.begin(), selected.end(), [](const auto& a, const auto& b) { return std::count(a.first.begin(), a.first.end(), '.') < std::count(b.first.begin(), b.first.end(), '.'); });
    for (const auto& [path, value] : selected) {
        bool ancestor_failed{}; for (const auto& [failed, messages] : result.errors) if (path == failed || (path.starts_with(failed) && path.size() > failed.size() && path[failed.size()] == '.')) { ancestor_failed = true; break; }
        if (ancestor_failed) continue;
        if (flat) { auto object = result.values.as_object(); object.insert_or_assign(path, value); result.values = Json::object(std::move(object)); } else result.values = select_path(result.values, input, path, value);
    }
    return result;
}
} // namespace

StructuredResult Validator::check(const Json& input, const Rules& rules) { return check(input, rules, {}); }
Json Validator::validate(const Json& input, const Rules& rules) { return validate(input, rules, {}); }
StructuredResult Validator::check(const Json& input, const Rules& rules, Context context) { return check_data(input, rules, context); }
Json Validator::validate(const Json& input, const Rules& rules, Context context) { auto result = check(input, rules, context); if (!result.valid()) throw ValidationException{std::move(result.errors)}; return std::move(result.values); }
Result Validator::check(const Input& input, const Rules& rules) {
    auto checked = check_data(http::make_json(input), rules, {}, true); Input values;
    for (const auto& [name, value] : checked.values.as_object()) values.emplace(name, value.string());
    return {std::move(values), std::move(checked.errors)};
}
Input Validator::validate(const Input& input, const Rules& rules) { auto result = check(input, rules); if (!result.valid()) throw ValidationException{std::move(result.errors)}; return std::move(result.values); }
struct Engine::State { std::mutex mutex; std::unordered_map<String, CustomRule> rules; };
Engine::Engine() : state_(std::make_shared<State>()) {}
Engine::Engine(const std::shared_ptr<Engine>& registry) {
    if (!registry) throw std::invalid_argument("Validation registry is not configured");
    state_ = registry->state_;
}
void Engine::extend(String name, String message, Predicate predicate) const {
    if (!rule_identifier(name) || message.empty() || !predicate) throw std::invalid_argument("Custom validation requires a name, message and predicate");
    std::lock_guard lock{state_->mutex}; if (!state_->rules.emplace(std::move(name), CustomRule{std::move(message), std::move(predicate)}).second) throw std::logic_error("Custom validation rule is already registered");
}
Engine::CustomRule Engine::rule(View name) const { std::lock_guard lock{state_->mutex}; const auto found = state_->rules.find(String{name}); if (found == state_->rules.end()) throw std::logic_error("Unknown custom validation rule '" + String{name} + "'"); return found->second; }
StructuredResult Engine::check(const Json& input, const Rules& rules) const { return Validator::check(input, rules, {nullptr, this}); }
Json Engine::validate(const Json& input, const Rules& rules) const { return Validator::validate(input, rules, {nullptr, this}); }
} // namespace gungnir::validation
