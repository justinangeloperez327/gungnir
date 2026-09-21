#pragma once

#include <stdexcept>

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

} // namespace gungnir
