#pragma once

#include <string_view>

namespace gungnir::http {

enum class Method {
    get,
    post,
    put,
    patch,
    delete_,
    options,
    head
};

[[nodiscard]] constexpr std::string_view to_string(Method method) noexcept {
    switch (method) {
        case Method::get: return "GET";
        case Method::post: return "POST";
        case Method::put: return "PUT";
        case Method::patch: return "PATCH";
        case Method::delete_: return "DELETE";
        case Method::options: return "OPTIONS";
        case Method::head: return "HEAD";
    }
    return "UNKNOWN";
}

} // namespace gungnir::http
