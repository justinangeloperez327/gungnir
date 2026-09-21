#pragma once

#include <exception>

#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>

namespace gungnir::http {

class ExceptionHandler {
public:
    [[nodiscard]] Response render(
        Request& request,
        std::exception_ptr error
    ) const noexcept;
};

} // namespace gungnir::http
