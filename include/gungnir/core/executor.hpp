#pragma once
#include <chrono>
#include <exception>
#include <gungnir/core/resume_slot.hpp>
#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include <gungnir/database/runtime.hpp>
#include <gungnir/observability/trace.hpp>
#include <gungnir/view/runtime.hpp>

namespace gungnir {

class Executor {
public:
    explicit Executor(std::size_t workers = 0, std::size_t capacity = 4096);
    [[nodiscard]] std::exception_ptr failure() const;
    void rethrow_failure() const;
    [[nodiscard]] std::size_t pending() const noexcept;
    ~Executor();
    Executor(const Executor&) = delete;
    Executor& operator=(const Executor&) = delete;

    void start();
    void stop() noexcept;
    void join() noexcept;
    void post(std::function<void()> work);
    void schedule(std::coroutine_handle<> handle);

    [[nodiscard]] bool running() const noexcept;

    struct ScheduleAwaiter {
        Executor& executor;
        std::shared_ptr<detail::ResumeSlot> slot;
        ~ScheduleAwaiter() { if (slot) slot->cancel(); }
        bool await_ready() const noexcept { return false; }
        void await_suspend(
            std::coroutine_handle<> handle
        ) {
            slot = std::make_shared<detail::ResumeSlot>(handle);
            executor.post([state = slot] { state->resume(); });

        }
        void await_resume() const noexcept {}
    };

    [[nodiscard]] ScheduleAwaiter yield() noexcept { return {*this, {}}; }

private:
    void worker_loop() noexcept;
    std::size_t worker_count_;
    std::size_t capacity_;
    std::exception_ptr failure_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::function<void()>> queue_;
    std::vector<std::thread> workers_;
    bool running_{false};
    bool stopping_{false};
};

} // namespace gungnir

