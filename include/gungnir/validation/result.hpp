#pragma once

#include <gungnir/validation/rules.hpp>
#include <gungnir/http/json.hpp>

namespace gungnir::validation {

struct StructuredResult {
    http::Json values;
    Errors errors;
    [[nodiscard]] bool valid() const noexcept { return errors.empty(); }
    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
};

struct Result {
    Input values;
    Errors errors;

    [[nodiscard]] bool valid() const noexcept {
        return errors.empty();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return valid();
    }
};

} // namespace gungnir::validation
