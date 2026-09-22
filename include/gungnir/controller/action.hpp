#pragma once
#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <gungnir/core/task.hpp>
#include <gungnir/http/response.hpp>

namespace gungnir::controller {

template <typename>
inline constexpr bool unsupported_action_result = false;

template <typename Result>
[[nodiscard]] Task<http::Response> normalize(Result&& result) {
    using Value = std::remove_cvref_t<Result>;
    if constexpr (std::same_as<Value, http::Response>) {
        co_return std::forward<Result>(result);
    } else if constexpr (std::same_as<Value, Task<http::Response>>) {
        co_return co_await std::forward<Result>(result);
    } else if constexpr (std::same_as<Value, std::string>) {
        co_return http::Response::text(std::forward<Result>(result));
    } else if constexpr (std::same_as<Value, std::string_view>) {
        co_return http::Response::text(std::string{result});
    } else {
        static_assert(unsupported_action_result<Value>,
            "Controller action must return Response, Task<Response>, string, or string_view");
    }
}

} // namespace gungnir::controller
