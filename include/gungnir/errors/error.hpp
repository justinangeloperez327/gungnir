#pragma once
#include <stdexcept>
#include <string>
#include <utility>

namespace gungnir::errors {

class Error : public std::runtime_error {
public:
    explicit Error(std::string message, std::string code = {})
        : std::runtime_error(std::move(message)), code_(std::move(code)) {}

    [[nodiscard]] const std::string& code() const noexcept { return code_; }

private:
    std::string code_;
};

class ConfigurationError : public Error {
public:
    explicit ConfigurationError(std::string message)
        : Error(std::move(message), "configuration") {}
};

class RuntimeError : public Error {
public:
    explicit RuntimeError(std::string message)
        : Error(std::move(message), "runtime") {}
};

} // namespace gungnir::errors
