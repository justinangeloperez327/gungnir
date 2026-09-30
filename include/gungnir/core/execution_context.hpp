#pragma once

#include <memory>
#include <coroutine>
#include <type_traits>
#include <string>
#include <utility>

namespace gungnir::view { class Engine; }
namespace gungnir::database { class Connection; }
namespace gungnir::observability {
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
    std::shared_ptr<view::Engine> view;
    std::shared_ptr<database::Connection> database;
    observability::TraceContext trace;
};
using ContextHandle = std::shared_ptr<ExecutionContext>;
inline thread_local ContextHandle active_context = std::make_shared<ExecutionContext>();
inline ContextHandle current_execution_context() { return active_context; }

struct ContextPromise {
    ContextHandle context;
    ContextHandle caller;

    void prepare() {
        if (!context) context = std::make_shared<ExecutionContext>(*active_context);
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
