#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <gungnir/errors/error.hpp>
#include <gungnir/logging/logger.hpp>

#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>

namespace gungnir::http {

class ExceptionHandler {
public:
    using Reporter = std::function<bool(Request&, const errors::Error&)>;
    using Renderer = std::function<std::optional<Response>(Request&, const errors::Error&)>;

    ExceptionHandler& debug(bool enabled);
    ExceptionHandler& debug_when(std::function<bool()> resolver);
    ExceptionHandler& debug_allowed_when(std::function<bool()> policy);
    ExceptionHandler& logger(std::shared_ptr<logging::Logger> logger);
    ExceptionHandler& on_report(Reporter reporter);
    ExceptionHandler& on_render(Renderer renderer);

    // A true reporter result suppresses default logging. A null renderer
    // result falls through to the next renderer or the default response.
    template<class E> ExceptionHandler& report(std::function<bool(const E&, Request&)> callback) {
        reporters_.push_back([callback=std::move(callback)](Request& request, std::exception_ptr exception) {
            bool handled = false;
            errors::visit_exception<E>(exception,[&](const E& error) { handled = callback(error,request); });
            return handled;
        });
        return *this;
    }
    template<class E> ExceptionHandler& render(std::function<std::optional<Response>(const E&, Request&)> callback) {
        renderers_.push_back([callback=std::move(callback)](Request& request, std::exception_ptr exception) -> std::optional<Response> {
            std::optional<Response> result;
            errors::visit_exception<E>(exception,[&](const E& error) { result = callback(error,request); });
            return result;
        });
        return *this;
    }
    template<class E> ExceptionHandler& dont_report() {
        ignored_.push_back([](std::exception_ptr exception) { return errors::visit_exception<E>(exception,[](const E&) {}); });
        return *this;
    }
    void report(Request& request, std::exception_ptr error) const noexcept;
    [[nodiscard]] Response render(
        Request& request,
        std::exception_ptr error
    ) const;
private:
    void write_log(Request& request, const errors::Error& error) const noexcept;
    std::function<bool()> debug_ = [] { return false; };
    std::function<bool()> debug_allowed_ = [] { return true; };
    std::shared_ptr<logging::Logger> logger_;
    std::vector<std::function<bool(Request&, std::exception_ptr)>> reporters_;
    std::vector<std::function<std::optional<Response>(Request&, std::exception_ptr)>> renderers_;
    std::vector<std::function<bool(std::exception_ptr)>> ignored_;
};

} // namespace gungnir::http
