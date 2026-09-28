#pragma once

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/http/json.hpp>
#include <gungnir/http/method.hpp>
#include <gungnir/validation/rules.hpp>
#include <gungnir/validation/result.hpp>

namespace gungnir {
class ServiceScope;
}

namespace gungnir::routing {
class Router;
}

namespace gungnir::http {

class Request {
public:
    using Headers = std::unordered_map<std::string, std::string>;
    using Parameters = std::unordered_map<std::string, std::string>;
    using Input = std::unordered_map<std::string, std::string>;

    Request(
        Method method,
        std::string target,
        std::string body = {},
        CancellationToken cancellation = {}
    );

    [[nodiscard]] Method method() const noexcept;
    [[nodiscard]] std::string_view target() const noexcept;
    [[nodiscard]] std::string_view path() const noexcept;
    [[nodiscard]] std::string_view body() const noexcept;
    [[nodiscard]] bool cancelled() const noexcept;
    [[nodiscard]] CancellationToken cancellation() const noexcept;
    [[nodiscard]] bool has_services() const noexcept;
    [[nodiscard]] ServiceScope& services();
    [[nodiscard]] const ServiceScope& services() const;

    void set_header(std::string name, std::string value);
    [[nodiscard]] std::string_view header(
        std::string_view name
    ) const noexcept;
    [[nodiscard]] const Headers& headers() const noexcept;

    [[nodiscard]] std::string_view parameter(
        std::string_view name
    ) const noexcept;
    [[nodiscard]] bool has_parameter(
        std::string_view name
    ) const noexcept;
    [[nodiscard]] const Parameters& parameters() const noexcept;

    [[nodiscard]] std::string_view query(
        std::string_view name
    ) const noexcept;
    [[nodiscard]] const Input& query() const noexcept;

    [[nodiscard]] std::string_view cookie(
        std::string_view name
    ) const noexcept;
    [[nodiscard]] const Input& cookies() const noexcept;

    [[nodiscard]] const Json& json() const;
    [[nodiscard]] const Input& form() const noexcept;
    [[nodiscard]] bool expects_json() const noexcept;
    [[nodiscard]] bool is_json() const noexcept;
    [[nodiscard]] bool accepts(std::string_view media_type) const noexcept;
    [[nodiscard]] std::string_view content_type() const noexcept;
    [[nodiscard]] std::string_view user_agent() const noexcept;
    [[nodiscard]] std::string_view host() const noexcept;
    [[nodiscard]] std::string_view authorization() const noexcept;
    [[nodiscard]] bool bearer_authenticated() const noexcept;
    [[nodiscard]] std::string_view bearer_token() const noexcept;

    [[nodiscard]] std::string input(std::string_view name) const;
    [[nodiscard]] bool has(std::string_view name) const;
    [[nodiscard]] Input all() const;
    [[nodiscard]] Input only(
        std::initializer_list<std::string_view> names
    ) const;
    [[nodiscard]] Input except(
        std::initializer_list<std::string_view> names
    ) const;

    [[nodiscard]] Input validate(
        const validation::Rules& rules
    ) const;
    [[nodiscard]] validation::Result check(
        const validation::Rules& rules
    ) const;

private:
    friend class gungnir::routing::Router;

    void parse_target();
    void parse_body_input() const;
    void parse_cookies() const;
    void clear_route_parameters() noexcept;
    void set_route_parameter(std::string name, std::string value);
    void attach_services(std::shared_ptr<ServiceScope> services);

    Method method_;
    std::string target_;
    std::string path_;
    std::string body_;
    CancellationToken cancellation_;
    std::shared_ptr<ServiceScope> services_;
    Headers headers_;
    Parameters parameters_;
    Input query_;
    mutable Input form_;
    mutable Input cookies_;
    mutable std::optional<Json> json_;
    mutable bool body_parsed_{false};
    mutable bool cookies_parsed_{false};
};

} // namespace gungnir::http
