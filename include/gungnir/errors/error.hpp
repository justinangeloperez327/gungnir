#pragma once
#include <stdexcept>
#include <string>
#include <utility>
#include <exception>
#include <source_location>
#include <vector>
#include <optional>
#include <type_traits>
#include <cstddef>

namespace gungnir::errors {

class Error : public std::runtime_error {
public:
    explicit Error(std::string message, std::string code = {},
        std::source_location location = std::source_location::current());
    Error(std::string message, std::string code, std::string file, std::size_t line);

    [[nodiscard]] const std::string& code() const noexcept { return code_; }
    [[nodiscard]] std::string message() const { return what(); }
    [[nodiscard]] int status() const noexcept { return status_; }
    [[nodiscard]] const std::string& type() const noexcept { return type_; }
    [[nodiscard]] const std::string& file() const noexcept { return file_; }
    [[nodiscard]] std::size_t line() const noexcept { return line_; }
    [[nodiscard]] const std::vector<std::string>& trace() const noexcept { return trace_; }
    [[nodiscard]] static Error from_exception(std::exception_ptr exception);
    [[nodiscard]] std::exception_ptr exception() const { return original_ ? original_ : std::make_exception_ptr(*this); }
    [[noreturn]] void rethrow() const;

private:
    std::string code_;
    int status_{500};
    std::string type_{"Error"}, file_;
    std::size_t line_{0};
    std::vector<std::string> trace_;
    std::exception_ptr original_;
};

class ConfigurationError : public Error {
public:
    explicit ConfigurationError(std::string message, std::source_location location = std::source_location::current())
        : Error(std::move(message), "configuration", location) {}
};

class RuntimeError : public Error {
public:
    explicit RuntimeError(std::string message, std::source_location location = std::source_location::current())
        : Error(std::move(message), "runtime", location) {}
};

// A normalized Error catches every native failure. Specific native types
// retain their original payload, including validation field errors.
template<class E> [[nodiscard]] std::optional<E> catch_as(std::exception_ptr exception) {
    if constexpr (std::is_same_v<E, Error>) return Error::from_exception(exception);
    else {
        try { if (exception) std::rethrow_exception(exception); }
        catch (const E& error) { return error; }
        catch (...) {}
        return std::nullopt;
    }
}

template<class E, class Callback> bool visit_exception(std::exception_ptr exception, Callback&& callback) {
    if constexpr (std::is_same_v<E, Error>) { callback(Error::from_exception(exception)); return true; }
    else {
        try { if (exception) std::rethrow_exception(exception); }
        catch (const E& error) { callback(error); return true; }
        catch (...) {}
        return false;
    }
}

} // namespace gungnir::errors
