#pragma once
#include <utility>
#include <gungnir/validation/rules.hpp>

namespace gungnir::validation {

struct Result {
    Input values;
    Errors errors;
    [[nodiscard]] bool valid() const noexcept { return errors.empty(); }
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
};

} // namespace gungnir::validation
