#pragma once

#include <stdexcept>
#include <utility>

#include <gungnir/core/types.hpp>

namespace gungnir::orm {

class Error : public std::runtime_error {
public:
    explicit Error(String message) : std::runtime_error(std::move(message)) {}
};

class QueryError : public Error {
public:
    explicit QueryError(String message) : Error(std::move(message)) {}
};

} // namespace gungnir::orm
