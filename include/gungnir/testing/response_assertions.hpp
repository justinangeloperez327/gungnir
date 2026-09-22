#pragma once
#include <stdexcept>
#include <string>
#include <string_view>
#include <gungnir/http/response.hpp>

namespace gungnir::testing {

inline void assert_status(const http::Response& response, int expected) {
    if (response.status() != expected) {
        throw std::runtime_error("Expected HTTP status " + std::to_string(expected) +
            ", received " + std::to_string(response.status()));
    }
}

inline void assert_body_contains(const http::Response& response, std::string_view expected) {
    if (response.body().find(expected) == std::string_view::npos) {
        throw std::runtime_error("Response body does not contain expected text");
    }
}

inline void assert_header(const http::Response& response, std::string_view name, std::string_view expected) {
    if (response.header(name) != expected) {
        throw std::runtime_error("Response header does not match expected value: " + std::string{name});
    }
}

} // namespace gungnir::testing
