#include <gungnir/controller/controller.hpp>

#include <utility>

namespace gungnir {

Response Controller::response(String body, Integer status) {
    return Response{status, std::move(body)};
}

Response Controller::text(String body, Integer status) {
    return Response::text(std::move(body), status);
}

Response Controller::json(String body, Integer status) {
    Response result{status, std::move(body)};
    result.header("content-type", "application/json; charset=utf-8");
    return result;
}

Response Controller::no_content() {
    return Response{204};
}

Response Controller::redirect(String location, Integer status) {
    Response result{status};
    result.header("location", std::move(location));
    return result;
}

} // namespace gungnir
