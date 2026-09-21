#include <gungnir/http/request.hpp>

#include <utility>

namespace gungnir::http {

Request::Request(Method method, std::string path, std::string body)
    : method_(method), path_(std::move(path)), body_(std::move(body)) {}

Method Request::method() const noexcept { return method_; }

std::string_view Request::path() const noexcept { return path_; }

std::string_view Request::body() const noexcept { return body_; }

void Request::set_header(std::string name, std::string value) {
    headers_.insert_or_assign(std::move(name), std::move(value));
}

std::string_view Request::header(std::string_view name) const noexcept {
    const auto it = headers_.find(std::string{name});
    if (it == headers_.end()) {
        return {};
    }
    return it->second;
}

const Request::Headers& Request::headers() const noexcept { return headers_; }

std::string_view Request::parameter(std::string_view name) const noexcept {
    const auto found = parameters_.find(std::string{name});
    if (found == parameters_.end()) {
        return {};
    }

    return found->second;
}

bool Request::has_parameter(std::string_view name) const noexcept {
    return parameters_.contains(std::string{name});
}

const Request::Parameters& Request::parameters() const noexcept {
    return parameters_;
}

void Request::clear_route_parameters() noexcept {
    parameters_.clear();
}

void Request::set_route_parameter(std::string name, std::string value) {
    parameters_.insert_or_assign(std::move(name), std::move(value));
}

} // namespace gungnir::http
