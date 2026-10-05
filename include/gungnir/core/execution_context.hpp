#pragma once

#include <memory>
#include <vector>
#include <algorithm>
#include <coroutine>
#include <type_traits>
#include <string>
#include <utility>

namespace gungnir::view { class Engine; }
namespace gungnir { class Container; }
namespace gungnir::routing { class Router; }
namespace gungnir::http { class MiddlewareRegistry; }
namespace gungnir::database { class Connection; class Manager; }
namespace gungnir::observability {
class Tracer;
class Meter;
struct TraceContext {
    std::string trace_id;
    std::string span_id;
    [[nodiscard]] bool valid() const noexcept {
        return !trace_id.empty() && !span_id.empty();
    }
};
}

namespace gungnir::detail {
// Scopes retain their owner, so destroying a suspended task never modifies
// another task's ambient state. Only the active pointer is thread-local.
struct ExecutionContext {
    std::shared_ptr<routing::Router> router;
    std::shared_ptr<Container> container;
    std::shared_ptr<http::MiddlewareRegistry> middleware;
    std::shared_ptr<database::Manager> manager;
    std::shared_ptr<view::Engine> view;
    std::shared_ptr<database::Connection> database;
    observability::TraceContext trace;
    std::shared_ptr<observability::Tracer> tracer;
    std::shared_ptr<observability::Meter> meter;
};
using ContextHandle = std::shared_ptr<ExecutionContext>;
inline thread_local ContextHandle active_context = std::make_shared<ExecutionContext>();
inline ContextHandle current_execution_context() { return active_context; }

// Compatibility facades select the newest live application on this thread.
// Explicit scopes and request dispatch always select their owning application.
inline thread_local std::vector<std::weak_ptr<ExecutionContext>> applications;
inline ContextHandle application_context() {
    if (active_context->router) return active_context;
    while (!applications.empty()) {
        if (auto context = applications.back().lock()) return context;
        applications.pop_back();
    }
    return active_context;
}
inline ContextHandle capture_execution_context() {
    auto result = std::make_shared<ExecutionContext>(*active_context);
    if (!result->router) {
        const auto application = application_context();
        result->router = application->router;
        result->container = application->container;
        result->middleware = application->middleware;
        result->manager = application->manager;
        if (!result->view) result->view = application->view;
        if (!result->database) result->database = application->database;
        if (!result->trace.valid()) result->trace = application->trace;
        if (!result->tracer) result->tracer = application->tracer;
        if (!result->meter) result->meter = application->meter;
    }
    return result;
}
class ExecutionScope {
public:
    explicit ExecutionScope(const ContextHandle& context)
        : owner_(active_context), previous_(*owner_) {
        if (context) *owner_ = *context;
    }
    ExecutionScope(const ExecutionScope&) = delete;
    ExecutionScope& operator=(const ExecutionScope&) = delete;
    ExecutionScope(ExecutionScope&& other) noexcept
        : owner_(std::move(other.owner_)), previous_(std::move(other.previous_)) {}
    ~ExecutionScope() { if (owner_) *owner_ = std::move(previous_); }
private:
    ContextHandle owner_;
    ExecutionContext previous_;
};

struct ContextPromise {
    ContextHandle context;
    ContextHandle caller;

    void prepare() {
        if (!context) context = capture_execution_context();
    }
    void enter() {
        prepare();
        if (active_context != context) {
            caller = active_context;
            active_context = context;
        }
    }
    void leave() noexcept {
        if (active_context == context && caller) active_context = caller;
    }
    void finish() noexcept {
        leave();
        context.reset();
        caller.reset();
    }
    struct InitialAwaiter {
        ContextPromise& promise;
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<>) const noexcept {}
        void await_resume() const { promise.enter(); }
    };
    InitialAwaiter initial_suspend() noexcept { return {*this}; }

    template <typename A>
    struct ContextAwaiter {
        ContextPromise& promise;
        A awaiter;
        bool await_ready() { return awaiter.await_ready(); }
        template <typename P>
        decltype(auto) await_suspend(std::coroutine_handle<P> handle) {
            // Leave before publishing the handle: an awaiter may resume it on
            // another thread before await_suspend returns.
            promise.leave();
            try {
                using Result = decltype(awaiter.await_suspend(handle));
                if constexpr (std::is_same_v<Result, bool>) {
                    if (!awaiter.await_suspend(handle)) {
                        promise.enter();
                        return false;
                    }
                    return true;
                } else {
                    return awaiter.await_suspend(handle);
                }
            } catch (...) {
                promise.enter();
                throw;
            }
        }
        decltype(auto) await_resume() {
            promise.enter();
            return awaiter.await_resume();
        }
    };
    template <typename A>
    static decltype(auto) get_awaiter(A&& value) {
        if constexpr (requires { std::forward<A>(value).operator co_await(); })
            return std::forward<A>(value).operator co_await();
        else if constexpr (requires { operator co_await(std::forward<A>(value)); })
            return operator co_await(std::forward<A>(value));
        else return std::forward<A>(value);
    }
    template <typename A>
    auto await_transform(A&& value) {
        using Awaiter = decltype(get_awaiter(std::forward<A>(value)));
        // Own temporary awaiters; preserve references to noncopyable lvalues.
        using Stored = std::conditional_t<std::is_lvalue_reference_v<Awaiter>,
                                          Awaiter, std::remove_cvref_t<Awaiter>>;
        return ContextAwaiter<Stored>{*this, get_awaiter(std::forward<A>(value))};
    }
};
}
