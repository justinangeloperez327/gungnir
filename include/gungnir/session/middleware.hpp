#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <gungnir/http/cookie.hpp>
#include <gungnir/http/middleware.hpp>
#include <gungnir/session/store.hpp>

namespace gungnir::session {

struct Options {
    std::string cookie_name{
        "gungnir_session"
    };

    std::string path{"/"};
    std::optional<std::string> domain;
    bool secure{true};

    http::SameSite same_site{
        http::SameSite::lax
    };
};

class StartSession {
public:
    [[nodiscard]]
    static http::MiddlewareHandler make(
        std::shared_ptr<Store> store,
        Options options = {}
    );

private:
    static Task<http::Response> handle(
        http::Request& request,
        http::Next next,
        const std::shared_ptr<Store>& store,
        const Options& options
    );
};

[[nodiscard]]
inline http::MiddlewareHandler middleware(
    std::shared_ptr<Store> store,
    Options options = {}
) {
    return StartSession::make(
        std::move(store),
        std::move(options)
    );
}

} // namespace gungnir::session
