#pragma once

#include <gungnir/validation/rules.hpp>

namespace gungnir::validation {

class Validator {
public:
    [[nodiscard]] static Input validate(
        const Input& input,
        const Rules& rules
    );
};

} // namespace gungnir::validation
