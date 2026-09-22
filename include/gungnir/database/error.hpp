#pragma once

#include <stdexcept>
#include <string>
#include <utility>

#include <gungnir/core/types.hpp>
#include <gungnir/database/backend.hpp>

namespace gungnir::database {

class Error : public std::runtime_error {
public:
    Error(String message, Backend backend, String connection = {}, String statement = {})
        : std::runtime_error(message),
          backend_(backend),
          connection_(std::move(connection)),
          statement_(std::move(statement)) {}

    [[nodiscard]] Backend backend() const noexcept { return backend_; }
    [[nodiscard]] const String& connection() const noexcept { return connection_; }
    [[nodiscard]] const String& statement() const noexcept { return statement_; }

private:
    Backend backend_;
    String connection_;
    String statement_;
};

} // namespace gungnir::database
