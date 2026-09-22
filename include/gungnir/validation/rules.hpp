#pragma once

#include <cstddef>
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

    Rules& add(String field, String expression) {
        rules_.emplace_back(std::move(field), std::move(expression));
        return *this;
    }

    [[nodiscard]] bool empty() const noexcept { return rules_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return rules_.size(); }

    [[nodiscard]] const std::vector<Rule>& entries() const noexcept {
        return rules_;
    }

private:
    std::vector<Rule> rules_;
};

} // namespace gungnir::validation
