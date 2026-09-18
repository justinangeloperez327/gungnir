#include <gungnir/http/response.hpp>

#include <utility>

namespace gungnir::http {

Response::Response(int status, std::string body)
    : status_(status), body_(std::move(body)) {}

int Response::status() const noexcept { return status_; }

std::string_view Response::body() const noexcept { return body_; }

Response& Response::status(int value) noexcept {
    status_ = value;
    return *this;
}

Response& Response::body(std::string value) {
    body_ = std::move(value);
    return *this;
}

Response& Response::header(std::string name, std::string value) {
    headers_.insert_or_assign(std::move(name), std::move(value));
    return *this;
}

std::string_view Response::header(std::string_view name) const noexcept {
    const auto it = headers_.find(std::string{name});
    if (it == headers_.end()) {
        return {};
    }
    return it->second;
}

const Response::Headers& Response::headers() const noexcept { return headers_; }

Response Response::text(std::string body, int status) {
    Response response{status, std::move(body)};
    response.header("content-type", "text/plain; charset=utf-8");
    return response;
}

Response Response::not_found() {
    return Response::text("Not Found", 404);
}

} // namespace gungnir::http
