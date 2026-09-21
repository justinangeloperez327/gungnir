#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include <gungnir/core/container.hpp>
#include <gungnir/core/task.hpp>
#include <gungnir/core/types.hpp>
#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>

namespace gungnir::http {

using Next = std::function<Task<Response>(Request&)>;
using MiddlewareHandler =
    std::function<Task<Response>(Request&, Next)>;

class Middleware {
public:
    virtual ~Middleware() = default;

protected:
    [[nodiscard]] static Response response(
        String body = {},
        Integer status = 200
    ) {
        return Response{status, std::move(body)};
    }

    [[nodiscard]] static Response text(
        String body,
        Integer status = 200
    ) {
        return Response::text(std::move(body), status);
    }

    template <typename T>
    [[nodiscard]] static Response json(
        T&& value,
        Integer status = 200
    ) {
        return Response::json(
            std::forward<T>(value),
            status
        );
    }

    [[nodiscard]] static Response redirect(
        String location,
        Integer status = 302
    ) {
        return Response::redirect(
            std::move(location),
            status
        );
    }
};

template <typename>
inline constexpr bool unsupported_middleware_handler = false;

template <typename MiddlewareType>
[[nodiscard]] MiddlewareHandler make_middleware(
    Container& container
) {
    static_assert(
        std::derived_from<MiddlewareType, Middleware>,
        "Gungnir middleware must derive from Middleware"
    );

    return [&container](
        Request& request,
        Next next
    ) -> Task<Response> {
        auto middleware =
            container.template resolve<MiddlewareType>();

        if constexpr (
            requires(
                MiddlewareType& instance,
                Request& current_request,
                Next continuation
            ) {
                instance.handle(
                    current_request,
                    std::move(continuation)
                );
            }
        ) {
            using Result = decltype(
                std::declval<MiddlewareType&>().handle(
                    std::declval<Request&>(),
                    std::declval<Next>()
                )
            );

            if constexpr (std::same_as<Result, Response>) {
                co_return middleware->handle(
                    request,
                    std::move(next)
                );
            } else if constexpr (
                std::same_as<Result, Task<Response>>
            ) {
                co_return co_await middleware->handle(
                    request,
                    std::move(next)
                );
            } else {
                static_assert(
                    unsupported_middleware_handler<Result>,
                    "Middleware handle must return Response or Task<Response>"
                );
            }
        } else {
            static_assert(
                unsupported_middleware_handler<MiddlewareType>,
                "Middleware requires handle(Request&, Next)"
            );
        }
    };
}

} // namespace gungnir::http

namespace gungnir {

using Middleware = http::Middleware;
using Next = http::Next;

} // namespace gungnir
