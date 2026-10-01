#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <gungnir/core/task.hpp>
#include <gungnir/core/cancellation.hpp>

namespace gungnir {
namespace detail {
struct TaskCompletion {
    std::mutex mutex;
    std::condition_variable ready;
    bool finished{false};
    std::exception_ptr error;
};
struct TaskWaiter {
    struct promise_type {
        TaskCompletion& completion;
        promise_type(TaskCompletion& completion, Task<void>&) : completion(completion) {}
        TaskWaiter get_return_object() { return {std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        struct Final {
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise_type> frame) noexcept {
                auto& state = frame.promise().completion;
                std::lock_guard lock{state.mutex}; state.finished = true; state.ready.notify_all();
            }
            void await_resume() noexcept {}
        };
        Final final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() noexcept { completion.error = std::current_exception(); }
    };
    std::coroutine_handle<promise_type> frame;
    ~TaskWaiter() { frame.destroy(); }
};
inline TaskWaiter complete_task(TaskCompletion& state, Task<void>& task) { co_await task; }
}
inline void sync_wait(Task<void> task) {
    detail::TaskCompletion state;
    auto waiter = detail::complete_task(state,task);
    waiter.frame.resume();
    std::unique_lock lock{state.mutex}; state.ready.wait(lock,[&] { return state.finished; });
    if (state.error) std::rethrow_exception(state.error);
}
// Owns child tasks until completion, bounds child creation, and cancels siblings
// on failure. Cancellation is cooperative; destruction waits for owned work.
class TaskGroup {
public:
    explicit TaskGroup(std::size_t capacity = 64) : capacity_(capacity) {
        if (!capacity) throw std::invalid_argument("Task group capacity must be positive");
    }
    ~TaskGroup() { cancel(); join_all(); }
    TaskGroup(const TaskGroup&) = delete;
    TaskGroup& operator=(const TaskGroup&) = delete;
    void spawn(std::function<Task<void>(CancellationToken)> work) {
        if (!work) throw std::invalid_argument("Task group work cannot be empty");
        if (joined_ || token().cancelled()) throw std::logic_error("Task group is closed");
        if (threads_.size() >= capacity_) throw std::length_error("Task group capacity exceeded; join before starting another group");
        auto context = detail::capture_execution_context();
        threads_.emplace_back([this, context, work = std::move(work)] {
            detail::ExecutionScope scope{context};
            try { sync_wait(work(token())); }
            catch (const OperationCancelled&) { if (!token().cancelled()) remember_failure(std::current_exception()); }
            catch (...) { remember_failure(std::current_exception()); }
        });
    }
    void cancel() noexcept { cancellation_.cancel(); }
    [[nodiscard]] CancellationToken token() const noexcept { return cancellation_.token(); }
    void join() { join_all(); if (failure_) std::rethrow_exception(failure_); }
private:
    void remember_failure(std::exception_ptr error) {
        { std::lock_guard lock{mutex_}; if (!failure_) failure_ = std::move(error); }
        cancel();
    }
    void join_all() noexcept { for (auto& thread : threads_) if (thread.joinable()) thread.join(); joined_ = true; }
    std::size_t capacity_;
    CancellationSource cancellation_;
    std::vector<std::thread> threads_;
    std::mutex mutex_;
    std::exception_ptr failure_;
    bool joined_{false};
};
}
