#pragma once

#include <gungnir/core/types.hpp>
#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>

namespace gungnir {

using Request = http::Request;
using Response = http::Response;

class Controller {
protected:
    [[nodiscard]] static Response response(String body = {}, Integer status = 200);
    [[nodiscard]] static Response text(String body, Integer status = 200);
    [[nodiscard]] static Response json(String body, Integer status = 200);
    [[nodiscard]] static Response no_content();
    [[nodiscard]] static Response redirect(String location, Integer status = 302);
};

} // namespace gungnir
