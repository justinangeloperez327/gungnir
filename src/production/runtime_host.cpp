#include <gungnir/production/runtime_host.hpp>

#include <stdexcept>
#include <utility>

#include <gungnir/core/application.hpp>
#include <gungnir/production/runtime_adapters.hpp>
#include <gungnir/queue/worker.hpp>

namespace gungnir::production {

RuntimeHost::RuntimeHost(
    Application& application,
    SupervisorOptions options
)
    : application_(&application),
      supervisor_(
        std::move(options)
      ) {
    supervise(
        supervisor_,
        application,
        "http"
    );
}

RuntimeHost::~RuntimeHost() {
    request_stop();
    join();
}

RuntimeHost& RuntimeHost::queue(
    queue::Worker& worker,
    std::string name
) {
    ensure_not_started();

    supervise(
        supervisor_,
        worker,
        name
    );

    services_.push_back({
        std::move(name),
        [&worker](
            CancellationToken token
        ) {
            static_cast<void>(
                worker.run(
                    std::move(token)
                )
            );
        }
    });

    return *this;
}

RuntimeHost& RuntimeHost::scheduler(
    scheduler::Scheduler& scheduler,
    scheduler::RunnerOptions options,
    std::string name
) {
    ensure_not_started();

    supervise(
        supervisor_,
        scheduler,
        name
    );

    services_.push_back({
        std::move(name),
        [
            &scheduler,
            options
        ](
            CancellationToken token
        ) {
            static_cast<void>(
                scheduler.run(
                    options,
                    std::move(token)
                )
            );
        }
    });

    return *this;
}

void RuntimeHost::start() {
    bool expected = false;

    if (
        !started_
            .compare_exchange_strong(
                expected,
                true,
                std::memory_order_acq_rel
            )
    ) {
        throw std::logic_error(
            "Runtime host is already started"
        );
    }

    threads_.reserve(
        services_.size()
    );

    try {
        for (
            const auto& service :
            services_
        ) {
            threads_.emplace_back(
                [
                    this,
                    run = service.run
                ]() mutable {
                    try {
                        run(
                            supervisor_.token()
                        );
                    } catch (...) {
                        remember_failure(
                            std::current_exception()
                        );

                        supervisor_
                            .request_stop();
                    }

                    supervisor_.notify();
                }
            );
        }
    } catch (...) {
        supervisor_.request_stop();
        join();
        throw;
    }
}

ShutdownResult RuntimeHost::shutdown() {
    auto result =
        supervisor_.shutdown();

    join();

    return result;
}

void RuntimeHost::request_stop()
    noexcept {
    supervisor_.request_stop();
}

void RuntimeHost::join()
    noexcept {
    for (auto& thread : threads_) {
        if (
            thread.joinable()
        ) {
            thread.join();
        }
    }

    threads_.clear();
}

bool RuntimeHost::started()
    const noexcept {
    return
        started_.load(
            std::memory_order_acquire
        );
}

bool RuntimeHost::stopping()
    const noexcept {
    return supervisor_.stopping();
}

CancellationToken RuntimeHost::token()
    const noexcept {
    return supervisor_.token();
}

Supervisor& RuntimeHost::supervisor()
    noexcept {
    return supervisor_;
}

const Supervisor&
RuntimeHost::supervisor()
    const noexcept {
    return supervisor_;
}

std::exception_ptr RuntimeHost::failure()
    const noexcept {
    std::lock_guard lock{
        failure_mutex_
    };

    return failure_;
}

void RuntimeHost::rethrow_failure()
    const {
    const auto captured =
        failure();

    if (captured) {
        std::rethrow_exception(
            captured
        );
    }
}

ShutdownResult RuntimeHost::run(
    std::uint16_t port,
    std::string host
) {
    start();

    try {
        application_->listen(
            port,
            std::move(host)
        );
    } catch (...) {
        const auto failure =
            std::current_exception();

        request_stop();
        static_cast<void>(
            shutdown()
        );

        std::rethrow_exception(
            failure
        );
    }

    auto result =
        shutdown();

    rethrow_failure();

    return result;
}

void RuntimeHost::ensure_not_started()
    const {
    if (started()) {
        throw std::logic_error(
            "Runtime services must be registered before the host starts"
        );
    }
}

void RuntimeHost::remember_failure(
    std::exception_ptr failure
) noexcept {
    if (!failure) {
        return;
    }

    std::lock_guard lock{
        failure_mutex_
    };

    if (!failure_) {
        failure_ =
            std::move(failure);
    }
}

} // namespace gungnir::production
