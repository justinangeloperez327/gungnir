#pragma once

#include <initializer_list>
#include <unordered_map>
#include <utility>
#include <vector>

#include <gungnir/core/types.hpp>

namespace gungnir::validation {

using Input = std::unordered_map<String, String>;
using Errors = std::unordered_map<String, std::vector<String>>;

struct Rule {
    String field;
    String expression;

    Rule(String field_name, String rule_expression)
        : field(std::move(field_name)),
          expression(std::move(rule_expression)) {}
};

class Rules {
public:
    Rules() = default;

    Rules(std::initializer_list<Rule> rules)
        : rules_(rules) {}

    [[nodiscard]] const std::vector<Rule>& entries() const noexcept {
        return rules_;
    }

private:
    std::vector<Rule> rules_;
};

} // namespace gungnir::validation
