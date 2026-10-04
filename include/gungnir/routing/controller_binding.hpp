#pragma once

#include <charconv>
#include <cmath>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <gungnir/controller/action.hpp>
#include <gungnir/controller/controller.hpp>
#include <gungnir/core/container.hpp>
#include <gungnir/http/errors.hpp>
#include <gungnir/orm/orm.hpp>
#include <gungnir/routing/router.hpp>

namespace gungnir::routing {
namespace detail {
template<class> struct ActionParameters;
template<class R, class C, class... A> struct ActionParameters<R(C::*)(A...)> { using types = std::tuple<A...>; };
template<class R, class C, class... A> struct ActionParameters<R(C::*)(A...) const> : ActionParameters<R(C::*)(A...)> {};
template<class R, class C, class... A> struct ActionParameters<R(C::*)(A...) noexcept> : ActionParameters<R(C::*)(A...)> {};
template<class R, class C, class... A> struct ActionParameters<R(C::*)(A...) const noexcept> : ActionParameters<R(C::*)(A...)> {};

template<class T> T route_scalar(std::string_view value) {
    if constexpr (std::same_as<T,String>) return String{value};
    else if constexpr (std::same_as<T,bool>) {
        if (value == "true" || value == "1") return true;
        if (value == "false" || value == "0") return false;
        throw http::NotFoundException{};
    } else if constexpr (std::integral<T> || std::floating_point<T>) {
        T result{};
        const auto parsed = std::from_chars(value.data(),value.data()+value.size(),result);
        if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data()+value.size()) throw http::NotFoundException{};
        if constexpr (std::floating_point<T>) if (!std::isfinite(result)) throw http::NotFoundException{};
        return result;
    } else {
        static_assert(std::same_as<T,void>,"Route scalar type is unsupported");
    }
}

template<class Argument> auto route_argument(Request& request, const String& name) {
    using T = std::remove_cvref_t<Argument>;
    if constexpr (std::same_as<T,Request>) return std::ref(request);
    else {
        if (!request.has_parameter(name)) throw http::NotFoundException{};
        const auto value = request.parameter(name);
        if constexpr (requires { T::primary_key_name(); T::find_or_fail(model::AttributeValue{}); }) {
            std::optional<T> result;
            model::for_each_attribute<T>([&](const auto& descriptor) {
                using Member = std::remove_cvref_t<decltype(std::declval<T>().*(descriptor.member))>;
                if constexpr (model::attribute_kind<Member>::value == model::AttributeKind::primary_key) {
                    result = T::find_or_fail(model::to_value(route_scalar<typename Member::value_type>(value)));
                }
            });
            if (!result) throw std::logic_error("Route model metadata has no primary key");
            return std::move(*result);
        } else return route_scalar<T>(value);
    }
}

// A function coroutine owns the controller, bindings and model/scalar tuple.
// It does not borrow the registration lambda's closure across suspension.
template<class ControllerType, class Method, std::size_t... I>
Task<Response> invoke_bound_controller(Container* container, Method method,
    std::shared_ptr<const std::vector<String>> names, Request& request,
    std::index_sequence<I...>) {
    using Arguments = typename ActionParameters<Method>::types;
    auto owner = request.has_services() ? request.services().template resolve<ControllerType>() : container->template resolve<ControllerType>();
    auto values = std::tuple{route_argument<std::tuple_element_t<I,Arguments>>(request,names->at(I))...};
    co_return co_await controller::normalize(std::apply([&](auto&... value) {
        return std::invoke(method,*owner,value...);
    },values));
}
}

template<class ControllerType, class Method>
Handler bind_controller(Container& container, Method method, std::vector<String> names) {
    static_assert(std::derived_from<ControllerType,gungnir::Controller>);
    using Arguments = typename detail::ActionParameters<Method>::types;
    constexpr auto count = std::tuple_size_v<Arguments>;
    if (!method || names.size() != count) throw std::invalid_argument("Invalid controller route binding");
    auto bindings = std::make_shared<const std::vector<String>>(std::move(names));
    return [services=&container,method,bindings=std::move(bindings)](Request& request) {
        return detail::invoke_bound_controller<ControllerType>(services,method,bindings,request,std::make_index_sequence<count>{});
    };
}
}
