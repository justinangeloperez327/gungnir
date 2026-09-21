#include <gungnir/validation/validator.hpp>

#include <algorithm>
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

Input Validator::validate(
    const Input& input,
    const Rules& rules
) {
    Errors errors;
    Input validated;

    for (const auto& entry : rules.entries()) {
        const auto parsed = parse_rules(entry.expression);
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
                rule.name == "string"
            ) {
                continue;
            }

            bool valid = true;

            if (rule.name == "integer") {
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
            }
        }

        if (field_valid) {
            validated.insert_or_assign(
                entry.field,
                found->second
            );
        }
    }

    if (!errors.empty()) {
        throw ValidationException{
            std::move(errors)
        };
    }

    return validated;
}

} // namespace gungnir::validation
