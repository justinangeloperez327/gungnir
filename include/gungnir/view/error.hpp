#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace gungnir::view {

class Error : public std::runtime_error {
public:
    explicit Error(std::string message) : std::runtime_error(std::move(message)) {}
};

class NotFound final : public Error {
public:
    explicit NotFound(std::string name)
        : Error("Gungnir view not found: " + std::move(name)) {}
};

class SyntaxError final : public Error {
public:
    explicit SyntaxError(std::string message) : Error(std::move(message)) {}
};

} // namespace gungnir::view
