#pragma once

#include <stdexcept>
#include <utility>

#include <gungnir/validation/rules.hpp>

namespace gungnir::validation {

class ValidationException : public std::runtime_error {
public:
    explicit ValidationException(Errors errors)
        : std::runtime_error("Validation failed"),
          errors_(std::move(errors)) {}

    [[nodiscard]] const Errors& errors() const noexcept {
        return errors_;
    }

private:
    Errors errors_;
};

} // namespace gungnir::validation
