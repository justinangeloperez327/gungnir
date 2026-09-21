#pragma once

#include <stdexcept>
#include <utility>

#include <gungnir/core/types.hpp>

namespace gungnir::http {

class HttpException : public std::runtime_error {
public:
    HttpException(Integer status, String message)
        : std::runtime_error(message),
          status_(status) {}

    [[nodiscard]] Integer status() const noexcept {
        return status_;
    }

private:
    Integer status_;
};

class BadRequestException : public HttpException {
public:
    explicit BadRequestException(
        String message = "Bad Request"
    )
        : HttpException(400, std::move(message)) {}
};

class AuthenticationException : public HttpException {
public:
    explicit AuthenticationException(
        String message = "Unauthenticated"
    )
        : HttpException(401, std::move(message)) {}
};

class AuthorizationException : public HttpException {
public:
    explicit AuthorizationException(
        String message = "Forbidden"
    )
        : HttpException(403, std::move(message)) {}
};

class NotFoundException : public HttpException {
public:
    explicit NotFoundException(
        String message = "Not Found"
    )
        : HttpException(404, std::move(message)) {}
};

} // namespace gungnir::http
