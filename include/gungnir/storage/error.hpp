#pragma once
#include <stdexcept>
#include <string>
#include <utility>

namespace gungnir::storage {
class Error : public std::runtime_error {
public:
    explicit Error(std::string message) : std::runtime_error(std::move(message)) {}
};
class InvalidPath final : public Error {
public:
    explicit InvalidPath(std::string path) : Error("Invalid storage path: " + std::move(path)) {}
};
class NotFound final : public Error {
public:
    explicit NotFound(std::string path) : Error("Storage object not found: " + std::move(path)) {}
};
} // namespace gungnir::storage
