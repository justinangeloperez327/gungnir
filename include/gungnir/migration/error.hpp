#pragma once

#include <stdexcept>
#include <utility>

#include <gungnir/core/types.hpp>

namespace gungnir::migration {

class Error : public std::runtime_error {
public:
    explicit Error(String message) : std::runtime_error(std::move(message)) {}
};

class RegistrationError : public Error {
public:
    explicit RegistrationError(String message) : Error(std::move(message)) {}
};

} // namespace gungnir::migration
