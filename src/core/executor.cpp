#include <gungnir/core/executor.hpp>
#include <gungnir/database/runtime.hpp>
#include <gungnir/observability/trace.hpp>
#include <gungnir/view/runtime.hpp>
#include <algorithm>
#include <stdexcept>

namespace gungnir {

Executor::Executor(std::size_t workers, std::size_t capacity)
    : worker_count_(workers == 0 ? std::clamp<unsigned>(std::thread::hardware_concurrency(), 2U, 32U) : workers), capacity_(capacity) { if (!capacity) throw std::invalid_argument("Executor capacity must be positive"); }

Executor::~Executor() { stop(); join(); }

void Executor::start() {
    std::lock_guard lock{mutex_};
    if (running_) return;
    stopping_ = false;
    running_ = true;
    for (std::size_t i = 0; i < worker_count_; ++i) workers_.emplace_back([this] { worker_loop(); });
}

void Executor::stop() noexcept {
    { std::lock_guard lock{mutex_}; stopping_ = true; }
    ready_.notify_all();
}

void Executor::join() noexcept {
    for (auto& worker : workers_) if (worker.joinable()) worker.join();
    workers_.clear();
    std::lock_guard lock{mutex_};
    running_ = false;
}

void Executor::post(
    std::function<void()> work
) {
    if (!work) {
        return;
    }

    auto context = detail::capture_execution_context();
    auto wrapped = [context, work = std::move(work)]() mutable {
        detail::ExecutionScope scope{context};
        work();
    };

    {
        std::lock_guard lock{
            mutex_
        };

        if (stopping_) {
            throw std::logic_error(
                "Gungnir executor is stopping"
            );
        }

        if (queue_.size() >= capacity_) throw std::length_error("Gungnir executor queue is full");
        queue_.push_back(
            std::move(wrapped)
        );
    }

    ready_.notify_one();
}

std::exception_ptr Executor::failure() const { std::lock_guard lock{mutex_}; return failure_; }
void Executor::rethrow_failure() const { if (auto error = failure()) std::rethrow_exception(error); }
std::size_t Executor::pending() const noexcept { std::lock_guard lock{mutex_}; return queue_.size(); }

void Executor::schedule(std::coroutine_handle<> handle) { post([handle] { if (handle && !handle.done()) handle.resume(); }); }

bool Executor::running() const noexcept { std::lock_guard lock{mutex_}; return running_ && !stopping_; }

void Executor::worker_loop() noexcept {
    while (true) {
        std::function<void()> work;
        {
            std::unique_lock lock{mutex_};
            ready_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (queue_.empty()) { if (stopping_) return; continue; }
            work = std::move(queue_.front()); queue_.pop_front();
        }
        try { work(); } catch (...) { std::lock_guard lock{mutex_}; if (!failure_) failure_ = std::current_exception(); }
    }
}

} // namespace gungnir
