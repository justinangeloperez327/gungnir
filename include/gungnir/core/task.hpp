#pragma once

#include <concepts>
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace gungnir {

template <typename T>
class [[nodiscard]] Task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    Task() noexcept = default;
    explicit Task(handle_type handle) noexcept : handle_(handle) {}

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(handle_); }
    [[nodiscard]] bool done() const noexcept { return !handle_ || handle_.done(); }

    struct Awaiter {
        handle_type handle;

        [[nodiscard]] bool await_ready() const noexcept { return !handle || handle.done(); }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept {
            handle.promise().continuation = continuation;
            return handle;
        }

        T await_resume() {
            return handle.promise().result();
        }
    };

    [[nodiscard]] Awaiter operator co_await() & noexcept { return Awaiter{handle_}; }
    [[nodiscard]] Awaiter operator co_await() && noexcept { return Awaiter{handle_}; }

    struct promise_type {
        std::optional<T> value;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation{std::noop_coroutine()};

        [[nodiscard]] Task get_return_object() noexcept {
            return Task{handle_type::from_promise(*this)};
        }

        [[nodiscard]] std::suspend_always initial_suspend() const noexcept { return {}; }

        struct FinalAwaiter {
            [[nodiscard]] bool await_ready() const noexcept { return false; }

            std::coroutine_handle<> await_suspend(handle_type handle) const noexcept {
                return handle.promise().continuation;
            }

            void await_resume() const noexcept {}
        };

        [[nodiscard]] FinalAwaiter final_suspend() const noexcept { return {}; }

        template <typename U>
        requires std::convertible_to<U&&, T>
        void return_value(U&& result) {
            value.emplace(std::forward<U>(result));
        }

        void unhandled_exception() noexcept { exception = std::current_exception(); }

        T result() {
            if (exception) {
                std::rethrow_exception(exception);
            }
            if (!value) {
                throw std::logic_error("Gungnir Task completed without a value");
            }
            return std::move(*value);
        }
    };

private:
    handle_type handle_{};
};

template <>
class [[nodiscard]] Task<void> {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    Task() noexcept = default;
    explicit Task(handle_type handle) noexcept : handle_(handle) {}

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(handle_); }
    [[nodiscard]] bool done() const noexcept { return !handle_ || handle_.done(); }

    struct Awaiter {
        handle_type handle;

        [[nodiscard]] bool await_ready() const noexcept { return !handle || handle.done(); }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept {
            handle.promise().continuation = continuation;
            return handle;
        }

        void await_resume() { handle.promise().result(); }
    };

    [[nodiscard]] Awaiter operator co_await() & noexcept { return Awaiter{handle_}; }
    [[nodiscard]] Awaiter operator co_await() && noexcept { return Awaiter{handle_}; }

    struct promise_type {
        std::exception_ptr exception;
        std::coroutine_handle<> continuation{std::noop_coroutine()};

        [[nodiscard]] Task get_return_object() noexcept {
            return Task{handle_type::from_promise(*this)};
        }

        [[nodiscard]] std::suspend_always initial_suspend() const noexcept { return {}; }

        struct FinalAwaiter {
            [[nodiscard]] bool await_ready() const noexcept { return false; }

            std::coroutine_handle<> await_suspend(handle_type handle) const noexcept {
                return handle.promise().continuation;
            }

            void await_resume() const noexcept {}
        };

        [[nodiscard]] FinalAwaiter final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() noexcept { exception = std::current_exception(); }

        void result() {
            if (exception) {
                std::rethrow_exception(exception);
            }
        }
    };

private:
    handle_type handle_{};
};

} // namespace gungnir
