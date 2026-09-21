#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include <gungnir/view/data.hpp>

namespace gungnir::http {

class Response {
public:
    using Headers = std::unordered_map<std::string, std::string>;

    Response(int status = 200, std::string body = {});

    [[nodiscard]] int status() const noexcept;
    [[nodiscard]] std::string_view body() const noexcept;

    Response& status(int value) noexcept;
    Response& body(std::string value);
    Response& header(std::string name, std::string value);

    [[nodiscard]] std::string_view header(std::string_view name) const noexcept;
    [[nodiscard]] const Headers& headers() const noexcept;

    [[nodiscard]] static Response text(std::string body, int status = 200);
    [[nodiscard]] static Response view(
        std::string name,
        gungnir::view::Data data = {},
        int status = 200
    );
    [[nodiscard]] static Response not_found();

private:
    int status_;
    std::string body_;
    Headers headers_;
};

} // namespace gungnir::http
