#include <gungnir/errors/error.hpp>
#include <gungnir/http/errors.hpp>
#include <gungnir/model/errors.hpp>
#include <gungnir/validation/exception.hpp>
#include <gungnir/auth/authorize.hpp>
#include <cstdlib>
#include <cstdint>
#include <memory>
#include <typeinfo>
#if defined(__GNUG__)
#include <cxxabi.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif __has_include(<execinfo.h>)
#include <execinfo.h>
#endif

namespace gungnir::errors {
namespace {
std::string exception_name(const std::exception& exception) {
#if defined(__GNUG__)
    int status = 0;
    const std::unique_ptr<char, decltype(&std::free)> name{abi::__cxa_demangle(typeid(exception).name(),nullptr,nullptr,&status), &std::free};
    if (status == 0 && name) return name.get();
#endif
    return typeid(exception).name();
}
std::vector<std::string> capture_trace() {
    std::vector<std::string> result;
#if defined(_WIN32)
    void* frames[32]{};
    const auto count = CaptureStackBackTrace(1, 32, frames, nullptr);
    for (USHORT i = 0; i < count; ++i) result.push_back(std::to_string(reinterpret_cast<std::uintptr_t>(frames[i])));
#elif __has_include(<execinfo.h>)
    void* frames[32]{};
    const auto count = ::backtrace(frames, 32);
    const std::unique_ptr<char*, decltype(&std::free)> names{::backtrace_symbols(frames, count), &std::free};
    if (names) for (int i = 1; i < count; ++i) result.emplace_back(names.get()[i]);
#endif
    return result;
}
}
Error::Error(std::string message, std::string code, std::source_location location)
    : Error(std::move(message), std::move(code), location.file_name(), location.line()) {}
Error::Error(std::string message, std::string code, std::string file, std::size_t line)
    : std::runtime_error(std::move(message)), code_(std::move(code)), file_(std::move(file)), line_(line), trace_(capture_trace()) {}

Error Error::from_exception(std::exception_ptr exception) {
    try {
        if (exception) std::rethrow_exception(exception);
    } catch (const Error& error) {
        auto result = error;
        if (typeid(error) != typeid(Error)) result.type_ = exception_name(error);
        if (const auto* http = dynamic_cast<const http::HttpException*>(&error)) {
            result.status_ = static_cast<int>(http->status()); result.type_ = "HttpError";
            if (result.status_ < 400 || result.status_ > 599) result.status_ = 500;
        }
        if (!result.original_) result.original_ = exception;
        return result;
    } catch (const std::exception& error) {
        Error result{error.what(), {}, "", 0};
        result.type_ = exception_name(error);
        if (const auto* http = dynamic_cast<const http::HttpException*>(&error)) result.status_ = static_cast<int>(http->status());
        else if (dynamic_cast<const ModelNotFoundError*>(&error)) { result.status_ = 404; result.type_ = "ModelNotFoundError"; }
        else if (dynamic_cast<const validation::ValidationException*>(&error)) { result.status_ = 422; result.type_ = "ValidationError"; }
        else if (const auto* authorization = dynamic_cast<const auth::AuthorizationError*>(&error)) result.status_ = static_cast<int>(authorization->status());
        if (result.status_ < 400 || result.status_ > 599) result.status_ = 500;
        result.original_ = exception;
        return result;
    } catch (const std::string& message) {
        Error result{message, {}, "", 0}; result.original_ = exception; return result;
    } catch (...) {
        Error result{"Unknown exception", {}, "", 0}; result.original_ = exception; return result;
    }
    return Error{"Internal Server Error", {}, "", 0};
}
void Error::rethrow() const {
    if (original_) std::rethrow_exception(original_);
    throw *this;
}
}
