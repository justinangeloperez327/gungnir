#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <gungnir/http/json.hpp>
#include <gungnir/http/cookie.hpp>
#include <gungnir/view/data.hpp>

namespace gungnir::http {

class Response {
public:
    using Headers =
        std::unordered_map<std::string, std::string>;
    using Cookies = std::vector<Cookie>;

    Response(int status = 200, std::string body = {});

    [[nodiscard]] int status() const noexcept;
    [[nodiscard]] std::string_view body() const noexcept;

    Response& status(int value) noexcept;
    Response& body(std::string value);
    Response& header(std::string name, std::string value);
    Response& cookie(Cookie value);
    Response& without_cookie(std::string name, std::string path = "/");

    [[nodiscard]] std::string_view header(
        std::string_view name
    ) const noexcept;
    [[nodiscard]] const Headers& headers() const noexcept;
    [[nodiscard]] const Cookies& cookies() const noexcept;

    [[nodiscard]] static Response text(
        std::string body,
        int status = 200
    );

    [[nodiscard]] static Response json(
        Json value,
        int status = 200
    );

    template <typename T>
    requires (!std::same_as<std::remove_cvref_t<T>, Json>)
    [[nodiscard]] static Response json(
        T&& value,
        int status = 200
    ) {
        return json(
            make_json(std::forward<T>(value)),
            status
        );
    }

    [[nodiscard]] static Response view(
        std::string name,
        gungnir::view::Data data = {},
        int status = 200
    );

    [[nodiscard]] static Response no_content();

    [[nodiscard]] static Response redirect(
        std::string location,
        int status = 302
    );

    [[nodiscard]] static Response not_found();
    [[nodiscard]] static Response html(std::string body, int status = 200);
    [[nodiscard]] static Response download(std::string body, std::string filename, std::string content_type = "application/octet-stream", int status = 200);

private:
    int status_;
    std::string body_;
    Headers headers_;
    Cookies cookies_;
};

} // namespace gungnir::http
