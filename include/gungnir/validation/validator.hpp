#pragma once

#include <gungnir/validation/rules.hpp>
#include <gungnir/validation/result.hpp>

namespace gungnir::validation {

class Validator {
public:
    [[nodiscard]] static StructuredResult check(const http::Json& input, const Rules& rules);
    [[nodiscard]] static http::Json validate(const http::Json& input, const Rules& rules);
    [[nodiscard]] static Result check(const Input& input, const Rules& rules);
    [[nodiscard]] static Input validate(
        const Input& input,
        const Rules& rules
    );
};

} // namespace gungnir::validation
