#pragma once

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/scheduler/scheduler.hpp>
#include <gungnir/production/supervisor.hpp>

namespace gungnir {

class Application;

namespace queue {
class Worker;
}

namespace production {

class RuntimeHost {
public:
    explicit RuntimeHost(
        Application& application,
        SupervisorOptions options = {}
    );

    ~RuntimeHost();

    RuntimeHost(
        const RuntimeHost&
    ) = delete;

    RuntimeHost& operator=(
        const RuntimeHost&
    ) = delete;

    RuntimeHost& queue(
        queue::Worker& worker,
        std::string name = "queue"
    );

    RuntimeHost& scheduler(
        scheduler::Scheduler& scheduler,
        scheduler::RunnerOptions options = {},
        std::string name = "scheduler"
    );

    void start();

    [[nodiscard]]
    ShutdownResult shutdown();

    void request_stop()
        noexcept;

    void join()
        noexcept;

    [[nodiscard]]
    bool started()
        const noexcept;

    [[nodiscard]]
    bool stopping()
        const noexcept;

    [[nodiscard]]
    CancellationToken token()
        const noexcept;

    [[nodiscard]]
    Supervisor& supervisor()
        noexcept;

    [[nodiscard]]
    const Supervisor& supervisor()
        const noexcept;

    [[nodiscard]]
    std::exception_ptr failure()
        const noexcept;

    void rethrow_failure()
        const;

    [[nodiscard]]
    ShutdownResult run(
        std::uint16_t port = 8000,
        std::string host = "127.0.0.1"
    );

private:
    struct Service {
        std::string name;
        std::function<void(
            CancellationToken
        )> run;
    };

    void ensure_not_started()
        const;

    void remember_failure(
        std::exception_ptr failure
    ) noexcept;

    Application* application_;
    Supervisor supervisor_;
    std::vector<Service> services_;
    std::vector<std::thread> threads_;

    std::atomic_bool started_{
        false
    };

    mutable std::mutex failure_mutex_;
    std::exception_ptr failure_;
};

} // namespace production
} // namespace gungnir
