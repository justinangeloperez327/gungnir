#pragma once

#include <gungnir/http/request.hpp>
#include <gungnir/validation/rules.hpp>
#include <gungnir/validation/validator.hpp>

namespace gungnir::validation {

class ValidatedRequest {
public:
    explicit ValidatedRequest(const http::Request& request)
        : request_(&request) {}

    virtual ~ValidatedRequest() = default;

    [[nodiscard]] Input validated() const {
        return Validator::validate(
            request_->all(),
            rules()
        );
    }

    [[nodiscard]] const http::Request& request() const noexcept {
        return *request_;
    }

protected:
    [[nodiscard]] virtual Rules rules() const = 0;

private:
    const http::Request* request_;
};

} // namespace gungnir::validation

namespace gungnir {

using ValidatedRequest = validation::ValidatedRequest;

} // namespace gungnir
