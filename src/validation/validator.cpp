#include <gungnir/validation/validator.hpp>

#include <algorithm>
#include <unordered_set>
#include <gungnir/database/runtime.hpp>
#include <charconv>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <gungnir/validation/exception.hpp>

namespace gungnir::validation {

namespace {

struct ParsedRule {
    String name;
    String argument;
};

std::string_view trim(std::string_view value) {
    while (
        !value.empty() &&
        std::isspace(static_cast<unsigned char>(value.front())) != 0
    ) {
        value.remove_prefix(1);
    }

    while (
        !value.empty() &&
        std::isspace(static_cast<unsigned char>(value.back())) != 0
    ) {
        value.remove_suffix(1);
    }

    return value;
}

bool blank(std::string_view value) {
    return trim(value).empty();
}

std::vector<ParsedRule> parse_rules(std::string_view expression) {
    std::vector<ParsedRule> rules;
    std::size_t cursor = 0;

    while (cursor <= expression.size()) {
        const auto separator = expression.find('|', cursor);
        const auto end = separator == std::string_view::npos
            ? expression.size()
            : separator;

        const auto token = trim(expression.substr(cursor, end - cursor));

        if (!token.empty()) {
            const auto colon = token.find(':');

            rules.push_back(ParsedRule{
                String{
                    colon == std::string_view::npos
                        ? token
                        : token.substr(0, colon)
                },
                String{
                    colon == std::string_view::npos
                        ? std::string_view{}
                        : token.substr(colon + 1)
                }
            });
        }

        if (separator == std::string_view::npos) {
            break;
        }

        cursor = separator + 1;
    }

    return rules;
}

bool has_rule(
    const std::vector<ParsedRule>& rules,
    std::string_view name
) {
    return std::any_of(
        rules.begin(),
        rules.end(),
        [name](const ParsedRule& rule) {
            return rule.name == name;
        }
    );
}

bool integer_value(std::string_view value) {
    if (value.empty()) {
        return false;
    }

    long long parsed = 0;
    const auto result = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed
    );

    return
        result.ec == std::errc{} &&
        result.ptr == value.data() + value.size();
}

bool numeric_value(std::string_view value) {
    if (value.empty()) {
        return false;
    }

    double parsed = 0.0;
    const auto result = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed
    );

    return
        result.ec == std::errc{} &&
        result.ptr == value.data() + value.size() &&
        std::isfinite(parsed);
}

double number(std::string_view value) {
    double parsed = 0.0;
    const auto result = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed
    );

    if (
        result.ec != std::errc{} ||
        result.ptr != value.data() + value.size()
    ) {
        throw std::logic_error(
            "Validation numeric comparison received a non-numeric value"
        );
    }

    return parsed;
}

std::size_t unsigned_argument(
    std::string_view rule,
    std::string_view value
) {
    std::size_t parsed = 0;
    const auto result = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed
    );

    if (
        value.empty() ||
        result.ec != std::errc{} ||
        result.ptr != value.data() + value.size()
    ) {
        throw std::logic_error(
            "Validation rule '" +
            String{rule} +
            "' requires a non-negative integer argument"
        );
    }

    return parsed;
}

bool email_value(std::string_view value) {
    const auto at = value.find('@');

    if (
        at == std::string_view::npos ||
        at == 0 ||
        at + 1 >= value.size() ||
        value.find('@', at + 1) != std::string_view::npos
    ) {
        return false;
    }

    const auto dot = value.find('.', at + 1);

    return
        dot != std::string_view::npos &&
        dot > at + 1 &&
        dot + 1 < value.size();
}

bool boolean_value(std::string_view value) {
    return
        value == "true" ||
        value == "false" ||
        value == "1" ||
        value == "0" ||
        value == "yes" ||
        value == "no" ||
        value == "on" ||
        value == "off";
}

bool accepted_value(std::string_view value) {
    return
        value == "yes" ||
        value == "on" ||
        value == "1" ||
        value == "true";
}

bool in_values(
    std::string_view value,
    std::string_view argument
) {
    std::size_t cursor = 0;

    while (cursor <= argument.size()) {
        const auto separator = argument.find(',', cursor);
        const auto end = separator == std::string_view::npos
            ? argument.size()
            : separator;

        if (trim(argument.substr(cursor, end - cursor)) == value) {
            return true;
        }

        if (separator == std::string_view::npos) {
            break;
        }

        cursor = separator + 1;
    }

    return false;
}

void validate_rule_names(const std::vector<ParsedRule>& rules) {
    static const std::unordered_set<String> known{
        "required","present","nullable","sometimes","string","integer","numeric","boolean",
        "email","accepted","length","min","max","in","same","confirmed","unique","exists",
        "array","object","bail"
    };
    for (const auto& rule : rules)
        if (!known.contains(rule.name)) throw std::logic_error("Unknown validation rule '" + rule.name + "'");
}

bool database_rule(std::string_view value, const ParsedRule& rule, std::string_view field) {
    std::vector<String> arguments;
    std::string_view remaining{rule.argument};
    while (true) {
        auto comma = remaining.find(',');
        arguments.emplace_back(trim(remaining.substr(0, comma)));
        if (comma == std::string_view::npos) break;
        remaining.remove_prefix(comma + 1);
    }
    if (arguments.empty() || arguments.size() > (rule.name == "unique" ? 4U : 2U))
        throw std::invalid_argument("Invalid database validation rule arguments");
    auto connection = database::runtime::read_connection();
    const auto backend = connection->backend();
    if (backend == database::Backend::mongodb) throw std::logic_error("Database validation requires a SQL connection");
    auto identifier = [backend](std::string_view name) {
        if (name.empty() || !(std::isalpha(static_cast<unsigned char>(name.front())) || name.front() == '_') ||
            !std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; }))
            throw std::invalid_argument("Invalid database validation identifier");
        return backend == database::Backend::mysql ? "`" + String{name} + "`" : "\"" + String{name} + "\"";
    };
    const auto column = arguments.size() > 1 && !arguments[1].empty() ? arguments[1] : String{field};
    const auto placeholder = [backend](int index) { return backend == database::Backend::postgresql ? "$" + std::to_string(index) : "?"; };
    auto sql = "SELECT COUNT(*) AS matches FROM " + identifier(arguments[0]) + " WHERE " + identifier(column) + " = " + placeholder(1);
    std::vector<model::AttributeValue> bindings{String{value}};
    if (arguments.size() > 2 && !arguments[2].empty()) {
        sql += " AND " + identifier(arguments.size() > 3 ? arguments[3] : "id") + " <> " + placeholder(2);
        bindings.emplace_back(arguments[2]);
    }
    auto result = connection->execute(sql, bindings);
    if (result.rows.empty() || !result.rows.front().contains("matches")) throw std::runtime_error("Database validation returned no count");
    auto count = model::value_cast<UInt64>(result.rows.front().at("matches"));
    return rule.name == "exists" ? count != 0 : count == 0;
}

void add_error(
    Errors& errors,
    const String& field,
    String message
) {
    errors[field].push_back(std::move(message));
}

String message(
    const String& field,
    std::string_view rule,
    std::string_view argument = {}
) {
    if (rule == "required") {
        return "The " + field + " field is required.";
    }

    if (rule == "present") {
        return "The " + field + " field must be present.";
    }

    if (rule == "integer") {
        return "The " + field + " field must be an integer.";
    }

    if (rule == "numeric") {
        return "The " + field + " field must be numeric.";
    }

    if (rule == "boolean") {
        return "The " + field + " field must be true or false.";
    }

    if (rule == "email") {
        return "The " + field + " field must be a valid email address.";
    }

    if (rule == "min") {
        return "The " + field + " field must be at least " +
               String{argument} + ".";
    }

    if (rule == "max") {
        return "The " + field + " field must not be greater than " +
               String{argument} + ".";
    }

    if (rule == "length") {
        return "The " + field + " field must be exactly " +
               String{argument} + " characters.";
    }

    if (rule == "in") {
        return "The " + field + " field contains an invalid value.";
    }

    if (rule == "same") {
        return "The " + field + " field must match " +
               String{argument} + ".";
    }

    if (rule == "confirmed") {
        return "The " + field + " field confirmation does not match.";
    }

    if (rule == "accepted") {
        return "The " + field + " field must be accepted.";
    }

    return "The " + field + " field is invalid.";
}

} // namespace

Result Validator::check(
    const Input& input,
    const Rules& rules
) {
    Errors errors;
    Input validated;

    for (const auto& entry : rules.entries()) {
        const auto parsed = parse_rules(entry.expression);
        validate_rule_names(parsed);
        const auto found = input.find(entry.field);
        const bool present = found != input.end();
        const std::string_view value =
            present ? std::string_view{found->second} : std::string_view{};

        if (has_rule(parsed, "sometimes") && !present) {
            continue;
        }

        if (has_rule(parsed, "present") && !present) {
            add_error(
                errors,
                entry.field,
                message(entry.field, "present")
            );
            continue;
        }

        if (
            has_rule(parsed, "required") &&
            (!present || blank(value))
        ) {
            add_error(
                errors,
                entry.field,
                message(entry.field, "required")
            );
            continue;
        }

        if (
            (!present || blank(value)) &&
            has_rule(parsed, "nullable")
        ) {
            if (present) {
                validated.insert_or_assign(
                    entry.field,
                    found->second
                );
            }
            continue;
        }

        if (!present) {
            continue;
        }

        const bool numeric_rules =
            has_rule(parsed, "integer") ||
            has_rule(parsed, "numeric");

        bool field_valid = true;

        for (const auto& rule : parsed) {
            if (
                rule.name == "required" ||
                rule.name == "present" ||
                rule.name == "nullable" ||
                rule.name == "sometimes" ||
                rule.name == "string" || rule.name == "bail"
            ) {
                continue;
            }

            bool valid = true;

            if (rule.name == "unique" || rule.name == "exists") {
                valid = database_rule(value, rule, entry.field);
            } else if (rule.name == "array" || rule.name == "object") {
                valid = false; // Structured values require the Json overload.
            } else if (rule.name == "integer") {
                valid = integer_value(value);
            } else if (rule.name == "numeric") {
                valid = numeric_value(value);
            } else if (rule.name == "boolean") {
                valid = boolean_value(value);
            } else if (rule.name == "email") {
                valid = email_value(value);
            } else if (rule.name == "accepted") {
                valid = accepted_value(value);
            } else if (rule.name == "length") {
                valid =
                    value.size() ==
                    unsigned_argument("length", rule.argument);
            } else if (rule.name == "min") {
                if (numeric_rules) {
                    valid =
                        numeric_value(value) &&
                        number(value) >=
                            static_cast<double>(
                                unsigned_argument("min", rule.argument)
                            );
                } else {
                    valid =
                        value.size() >=
                        unsigned_argument("min", rule.argument);
                }
            } else if (rule.name == "max") {
                if (numeric_rules) {
                    valid =
                        numeric_value(value) &&
                        number(value) <=
                            static_cast<double>(
                                unsigned_argument("max", rule.argument)
                            );
                } else {
                    valid =
                        value.size() <=
                        unsigned_argument("max", rule.argument);
                }
            } else if (rule.name == "in") {
                valid = in_values(value, rule.argument);
            } else if (rule.name == "same") {
                const auto other = input.find(rule.argument);
                valid =
                    other != input.end() &&
                    other->second == value;
            } else if (rule.name == "confirmed") {
                const auto confirmation =
                    input.find(entry.field + "_confirmation");

                valid =
                    confirmation != input.end() &&
                    confirmation->second == value;
            } else {
                throw std::logic_error(
                    "Unknown validation rule '" +
                    rule.name +
                    "'"
                );
            }

            if (!valid) {
                field_valid = false;
                add_error(
                    errors,
                    entry.field,
                    message(
                        entry.field,
                        rule.name,
                        rule.argument
                    )
                );
                if (has_rule(parsed, "bail")) break;
            }
        }

        if (field_valid) {
            validated.insert_or_assign(
                entry.field,
                found->second
            );
        }
    }

    return Result{std::move(validated), std::move(errors)};
}

Input Validator::validate(
    const Input& input,
    const Rules& rules
) {
    auto result = check(input, rules);

    if (!result.errors.empty()) {
        throw ValidationException{
            std::move(result.errors)
        };
    }

    return std::move(result.values);
}

} // namespace gungnir::validation

namespace gungnir::validation {
namespace {
const http::Json* at_path(const http::Json& input, std::string_view path) {
    const auto dot = path.find('.');
    auto key = path.substr(0, dot);
    const http::Json* next{};
    if (input.is_object()) next = input.get(key);
    else if (input.is_array()) {
        std::size_t index{};
        auto [end, error] = std::from_chars(key.data(), key.data() + key.size(), index);
        if (error == std::errc{} && end == key.data() + key.size() && index < input.as_array().size()) next = &input.as_array()[index];
    }
    return next && dot != std::string_view::npos ? at_path(*next, path.substr(dot + 1)) : next;
}
http::Json select_path(http::Json output, const http::Json& input, std::string_view path, const http::Json& value) {
    const auto dot = path.find('.');
    const auto key = path.substr(0, dot);
    if (input.is_array()) {
        auto array = output.is_array() ? output.as_array() : http::Json::Array{};
        std::size_t index{};
        auto [end, error] = std::from_chars(key.data(), key.data() + key.size(), index);
        if (error != std::errc{} || end != key.data() + key.size() || index >= input.as_array().size()) return output;
        if (array.size() <= index) array.resize(index + 1);
        array[index] = dot == std::string_view::npos ? value : select_path(array[index], input.as_array()[index], path.substr(dot + 1), value);
        return http::Json::array(std::move(array));
    }
    auto object = output.is_object() ? output.as_object() : http::Json::Object{};
    auto* child = input.get(key);
    object[String{key}] = dot == std::string_view::npos ? value : select_path(object[String{key}], child ? *child : http::Json{}, path.substr(dot + 1), value);
    return http::Json::object(std::move(object));
}
void expand_paths(const http::Json& input, std::string_view pattern, String prefix, std::vector<String>& paths) {
    const auto star = pattern.find('*');
    if (star == std::string_view::npos) { paths.push_back(prefix + String{pattern}); return; }
    if ((star && pattern[star - 1] != '.') || (star + 1 < pattern.size() && pattern[star + 1] != '.'))
        throw std::invalid_argument("Validation wildcard must be a complete path segment");
    auto parent_path = prefix + String{pattern.substr(0, star ? star - 1 : 0)};
    auto* parent = parent_path.empty() ? &input : at_path(input, parent_path);
    if (!parent || !parent->is_array()) return;
    auto suffix = pattern.substr(star + 1);
    for (std::size_t i = 0; i < parent->as_array().size(); ++i)
        expand_paths(input, suffix, (parent_path.empty() ? "" : parent_path + ".") + std::to_string(i), paths);
}
}
StructuredResult Validator::check(const http::Json& input, const Rules& rules) {
    if (!input.is_object()) throw std::invalid_argument("Structured validation requires an object");
    StructuredResult result{http::Json::object({}), {}};
    for (const auto& entry : rules.entries()) {
        auto parsed = parse_rules(entry.expression);
        validate_rule_names(parsed);
        std::vector<String> paths;
        expand_paths(input, entry.field, "", paths);
        for (const auto& path : paths) {
            const auto* value = at_path(input, path);
            if (!value && has_rule(parsed, "sometimes")) continue;
            auto fail = [&](std::string_view rule) { result.errors[path].push_back(message(path, rule)); };
            if (!value) {
                if (has_rule(parsed, "required") || has_rule(parsed, "present")) fail("required");
                continue;
            }
            const bool empty = value->is_null() || (value->is_string() && blank(value->string())) ||
                (value->is_array() && value->as_array().empty()) || (value->is_object() && value->as_object().empty());
            if (empty && has_rule(parsed, "required")) { fail("required"); continue; }
            if (value->is_null() && has_rule(parsed, "nullable")) {
                result.values = select_path(result.values, input, path, *value); continue;
            }
            if ((has_rule(parsed,"array") && !value->is_array()) || (has_rule(parsed,"object") && !value->is_object()) ||
                (has_rule(parsed,"string") && !value->is_string()) || (value->is_null() && !parsed.empty())) {
                fail("type"); continue;
            }
            if (value->is_array() || value->is_object()) {
                const auto size = value->is_array() ? value->as_array().size() : value->as_object().size();
                for (const auto& rule : parsed) {
                    if (rule.name == "min" && size < unsigned_argument(rule.name,rule.argument)) fail("min");
                    else if (rule.name == "max" && size > unsigned_argument(rule.name,rule.argument)) fail("max");
                    else if (rule.name != "array" && rule.name != "object" && rule.name != "required" && rule.name != "present" &&
                        rule.name != "nullable" && rule.name != "sometimes" && rule.name != "bail" && rule.name != "min" && rule.name != "max") fail(rule.name);
                }
            } else {
                Input scalar{{path,value->string()}};
                for (const auto& rule : parsed) {
                    if (rule.name == "same") { if (auto* other = at_path(input,rule.argument)) scalar[rule.argument] = other->string(); }
                    if (rule.name == "confirmed") { if (auto* other = at_path(input,path+"_confirmation")) scalar[path+"_confirmation"] = other->string(); }
                }
                auto checked = check(scalar, Rules{{path,entry.expression}});
                if (!checked.valid()) result.errors[path] = std::move(checked.errors.at(path));
            }
            if (!result.errors.contains(path)) result.values = select_path(result.values, input, path, *value);
        }
    }
    return result;
}
http::Json Validator::validate(const http::Json& input, const Rules& rules) {
    auto result = check(input, rules);
    if (!result.valid()) throw ValidationException{std::move(result.errors)};
    return std::move(result.values);
}
}
