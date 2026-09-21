#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include <gungnir/http/method.hpp>

namespace gungnir::routing {
class Router;
}

namespace gungnir::http {

class Request {
public:
    using Headers = std::unordered_map<std::string, std::string>;
    using Parameters = std::unordered_map<std::string, std::string>;

    Request(Method method, std::string path, std::string body = {});

    [[nodiscard]] Method method() const noexcept;
    [[nodiscard]] std::string_view path() const noexcept;
    [[nodiscard]] std::string_view body() const noexcept;

    void set_header(std::string name, std::string value);
    [[nodiscard]] std::string_view header(std::string_view name) const noexcept;
    [[nodiscard]] const Headers& headers() const noexcept;

    [[nodiscard]] std::string_view parameter(
        std::string_view name
    ) const noexcept;

    [[nodiscard]] bool has_parameter(
        std::string_view name
    ) const noexcept;

    [[nodiscard]] const Parameters& parameters() const noexcept;

private:
    friend class gungnir::routing::Router;

    void clear_route_parameters() noexcept;
    void set_route_parameter(std::string name, std::string value);
    Method method_;
    std::string path_;
    std::string body_;
    Headers headers_;
    Parameters parameters_;
};

} // namespace gungnir::http
