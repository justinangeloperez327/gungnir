#pragma once
#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>

namespace gungnir::http {

class TerminableMiddleware {
public:
    virtual ~TerminableMiddleware() = default;
    virtual void terminate(Request& request, const Response& response) = 0;
};

} // namespace gungnir::http
