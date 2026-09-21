#pragma once

#include <stdexcept>
#include <utility>

#include <gungnir/core/types.hpp>

namespace gungnir {

class ModelMetadataError : public std::logic_error {
public:
    explicit ModelMetadataError(const String& message)
        : std::logic_error(message) {}
};

class MassAssignmentError : public std::logic_error {
public:
    explicit MassAssignmentError(const String& attribute)
        : std::logic_error(
            "Attribute '" + attribute + "' is not fillable"
        ) {}
};

class ModelNotFoundError : public std::out_of_range {
public:
    explicit ModelNotFoundError(String model)
        : std::out_of_range(
            "Gungnir ORM model was not found"
        ),
          model_(std::move(model)) {}

    [[nodiscard]] const String& model() const noexcept {
        return model_;
    }

private:
    String model_;
};

} // namespace gungnir
