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

Response Controller::view(
    String name,
    gungnir::view::Data data,
    Integer status
) {
    return Response::view(
        std::move(name),
        std::move(data),
        status
    );
}

Response Controller::html(String body, Integer status) {
    return Response::html(std::move(body), status);
}

Response Controller::download(String body, String filename, String content_type, Integer status) {
    return Response::download(std::move(body), std::move(filename), std::move(content_type), status);
}

Response Controller::no_content() {
    return Response::no_content();
}

Response Controller::redirect(String location, Integer status) {
    return Response::redirect(
        std::move(location),
        status
    );
}

} // namespace gungnir
