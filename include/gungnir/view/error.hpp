#pragma once

#include <stdexcept>
#include <cstddef>
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

class InvalidPath final : public Error {
public:
    explicit InvalidPath(
        std::string name
    )
        : Error(
            "Gungnir view path is invalid or escapes the configured root: " +
            std::move(name)
          ) {}
};

class SyntaxError final : public Error {
public:
    explicit SyntaxError(std::string message) : Error(std::move(message)) {}
    SyntaxError(std::string message, std::string name, std::size_t line, std::size_t column)
        : Error(name + ":" + std::to_string(line) + ":" + std::to_string(column) + ": " + message),
          name_(std::move(name)), line_(line), column_(column) {}
    [[nodiscard]] const std::string& template_name() const noexcept { return name_; }
    [[nodiscard]] std::size_t line() const noexcept { return line_; }
    [[nodiscard]] std::size_t column() const noexcept { return column_; }
private:
    std::string name_;
    std::size_t line_{0}, column_{0};
};

} // namespace gungnir::view
