#pragma once
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <gungnir/http/request.hpp>
#include <gungnir/routing/router.hpp>
#include <gungnir/testing/response_assertions.hpp>

namespace gungnir::testing {

namespace detail {
struct HttpResult {
    std::mutex mutex;
    std::condition_variable completed;
    std::optional<http::Response> response;
    std::exception_ptr error;
    bool ready = false;
};
struct HttpWaiter {
    struct promise_type {
        HttpResult& state;
        promise_type(HttpResult& state, routing::Router&, http::Request&) : state(state) {}
        HttpWaiter get_return_object() {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise_type> handle) const noexcept {
                auto& state = handle.promise().state;
                std::lock_guard lock{state.mutex};
                state.ready = true;
                state.completed.notify_one();
                // No frame accesses after releasing the mutex: the waiting
                // thread may now destroy this completed coroutine.
            }
            void await_resume() const noexcept {}
        };
        FinalAwaiter final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() noexcept { state.error = std::current_exception(); }
    };
    std::coroutine_handle<promise_type> handle;
    ~HttpWaiter() { handle.destroy(); }
};
inline HttpWaiter dispatch(HttpResult& state, routing::Router& router, http::Request& request) {
    state.response.emplace(co_await router.dispatch(request));
}
} // namespace detail

class Http {
public:
    explicit Http(routing::Router& router) : router_(&router) {}

    [[nodiscard]] http::Response get(std::string target) {
        return send(http::Method::get, std::move(target));
    }

    [[nodiscard]] http::Response post(std::string target, std::string body = {}) {
        return send(http::Method::post, std::move(target), std::move(body));
    }

    [[nodiscard]] http::Response send(http::Method method, std::string target, std::string body = {}) {
        http::Request request{method, std::move(target), std::move(body)};
        detail::HttpResult state;
        auto task = detail::dispatch(state, *router_, request);
        task.handle.resume();
        std::unique_lock lock{state.mutex};
        state.completed.wait(lock, [&] { return state.ready; });
        if (state.error) std::rethrow_exception(state.error);
        return std::move(*state.response);
    }

private:
    routing::Router* router_;
};

} // namespace gungnir::testing

