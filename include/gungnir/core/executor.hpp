#pragma once
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace gungnir {

class Executor {
public:
    explicit Executor(std::size_t workers = 0);
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
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle) const { executor.schedule(handle); }
        void await_resume() const noexcept {}
    };

    [[nodiscard]] ScheduleAwaiter yield() noexcept { return {*this}; }

private:
    void worker_loop() noexcept;
    std::size_t worker_count_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::function<void()>> queue_;
    std::vector<std::thread> workers_;
    bool running_{false};
    bool stopping_{false};
};

} // namespace gungnir
