#include <gungnir/http/response.hpp>

#include <algorithm>
#include <cctype>
#include <utility>

#include <gungnir/view/engine.hpp>
#include <gungnir/view/runtime.hpp>

namespace gungnir::http {

namespace {

std::string normalize_header_name(std::string_view name) {
    std::string normalized{name};
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );
    return normalized;
}

} // namespace

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
    headers_.insert_or_assign(
        normalize_header_name(name),
        std::move(value)
    );
    return *this;
}

std::string_view Response::header(std::string_view name) const noexcept {
    const auto it = headers_.find(normalize_header_name(name));
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

Response Response::view(
    std::string name,
    gungnir::view::Data data,
    int status
) {
    Response response{
        status,
        gungnir::view::runtime::engine().render(name, data)
    };
    response.header("content-type", "text/html; charset=utf-8");
    return response;
}

Response Response::not_found() {
    return Response::text("Not Found", 404);
}

} // namespace gungnir::http
