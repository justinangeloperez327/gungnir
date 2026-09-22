#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <gungnir/controller/controller.hpp>
#include <gungnir/core/container.hpp>
#include <gungnir/http/middleware.hpp>
#include <gungnir/routing/router.hpp>

namespace gungnir::routing {

namespace detail {

void bind_route_runtime(
    Router& router,
    Container& container
) noexcept;

void unbind_route_runtime(
    Router& router,
    Container& container
) noexcept;

[[nodiscard]] Router& route_router();
[[nodiscard]] Container& route_container();

template <typename>
inline constexpr bool unsupported_controller_handler = false;

template <typename ControllerType, typename Method>
[[nodiscard]] Handler controller_handler(Method method) {
    auto* container = &route_container();

    return [container, method](
        http::Request& request
    ) -> Task<http::Response> {
        auto controller =
            container->template resolve<ControllerType>();

        if constexpr (
            std::invocable<
                Method,
                ControllerType&,
                http::Request&
            >
        ) {
            using Result = std::invoke_result_t<
                Method,
                ControllerType&,
                http::Request&
            >;

            if constexpr (
                std::same_as<Result, http::Response>
            ) {
                co_return std::invoke(
                    method,
                    *controller,
                    request
                );
            } else if constexpr (
                std::same_as<
                    Result,
                    Task<http::Response>
                >
            ) {
                co_return co_await std::invoke(
                    method,
                    *controller,
                    request
                );
            } else {
                static_assert(
                    unsupported_controller_handler<Result>,
                    "Controller action must return Response or Task<Response>"
                );
            }
        } else if constexpr (
            std::invocable<
                Method,
                ControllerType&
            >
        ) {
            using Result = std::invoke_result_t<
                Method,
                ControllerType&
            >;

            if constexpr (
                std::same_as<Result, http::Response>
            ) {
                co_return std::invoke(
                    method,
                    *controller
                );
            } else if constexpr (
                std::same_as<
                    Result,
                    Task<http::Response>
                >
            ) {
                co_return co_await std::invoke(
                    method,
                    *controller
                );
            } else {
                static_assert(
                    unsupported_controller_handler<Result>,
                    "Controller action must return Response or Task<Response>"
                );
            }
        } else {
            static_assert(
                unsupported_controller_handler<Method>,
                "Controller action must accept no arguments or Request&"
            );
        }
    };
}

} // namespace detail

template <typename MiddlewareType>
RouteRegistration& RouteRegistration::middleware() {
    return middleware(
        http::make_middleware<MiddlewareType>(
            detail::route_container()
        )
    );
}

class Route {
public:
    template <typename ControllerType, typename Method>
    static RouteRegistration get(
        std::string path,
        Method method
    ) {
        return add<ControllerType>(
            http::Method::get,
            std::move(path),
            method
        );
    }

    template <typename ControllerType, typename Method>
    static RouteRegistration post(
        std::string path,
        Method method
    ) {
        return add<ControllerType>(
            http::Method::post,
            std::move(path),
            method
        );
    }

    template <typename ControllerType, typename Method>
    static RouteRegistration put(
        std::string path,
        Method method
    ) {
        return add<ControllerType>(
            http::Method::put,
            std::move(path),
            method
        );
    }

    template <typename ControllerType, typename Method>
    static RouteRegistration patch(
        std::string path,
        Method method
    ) {
        return add<ControllerType>(
            http::Method::patch,
            std::move(path),
            method
        );
    }

    template <typename ControllerType, typename Method>
    static RouteRegistration options(std::string path, Method method) {
        return add<ControllerType>(http::Method::options, std::move(path), method);
    }

    template <typename ControllerType, typename Method>
    static RouteRegistration head(std::string path, Method method) {
        return add<ControllerType>(http::Method::head, std::move(path), method);
    }

    template <typename ControllerType, typename Method>
    static RouteRegistration remove(
        std::string path,
        Method method
    ) {
        return add<ControllerType>(
            http::Method::delete_,
            std::move(path),
            method
        );
    }

private:
    template <typename ControllerType, typename Method>
    static RouteRegistration add(
        http::Method verb,
        std::string path,
        Method method
    ) {
        static_assert(
            std::derived_from<
                ControllerType,
                gungnir::Controller
            >,
            "Gungnir controller routes require a Controller-derived type"
        );

        return detail::route_router().add(
            verb,
            std::move(path),
            detail::controller_handler<ControllerType>(
                method
            )
        );
    }
};

} // namespace gungnir::routing

namespace gungnir {

using Route = routing::Route;

} // namespace gungnir
