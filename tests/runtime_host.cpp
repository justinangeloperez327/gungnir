#include <cassert>
#include <chrono>
#include <stdexcept>
#include <thread>

#include <gungnir/core/application.hpp>
#include <gungnir/production/runtime_host.hpp>
#include <gungnir/queue/memory_driver.hpp>
#include <gungnir/queue/worker.hpp>

int main() {
    using namespace std::chrono_literals;

    gungnir::production::
        SupervisorOptions options;

    options.shutdown_timeout = 1s;
    options.poll_interval = 2ms;

    gungnir::CancellationSource
        pre_cancelled;

    pre_cancelled.cancel();

    gungnir::Application
        cancelled_application;

    cancelled_application.listen(
        0,
        "127.0.0.1",
        pre_cancelled.token()
    );

    assert(
        !cancelled_application
            .is_running()
    );

    gungnir::Application application;

    gungnir::queue::MemoryDriver
        queue_driver;

    gungnir::queue::WorkerOptions
        worker_options;

    worker_options.idle_sleep = 5s;

    gungnir::queue::Worker worker{
        queue_driver,
        worker_options
    };

    gungnir::scheduler::SystemClock
        scheduler_clock;

    gungnir::scheduler::Scheduler
        scheduler{
            scheduler_clock
        };

    gungnir::scheduler::RunnerOptions
        scheduler_options;

    scheduler_options.maximum_sleep =
        5s;

    gungnir::production::RuntimeHost host{
        application,
        options
    };

    host
        .queue(
            worker,
            "queue-primary"
        )
        .scheduler(
            scheduler,
            scheduler_options,
            "scheduler-primary"
        );

    host.start();

    for (
        int attempt = 0;
        attempt < 500 &&
        (
            !worker.running() ||
            !scheduler.running()
        );
        ++attempt
    ) {
        std::this_thread::sleep_for(
            1ms
        );
    }

    assert(host.started());
    assert(worker.running());
    assert(scheduler.running());
    assert(
        !host.token().cancelled()
    );

    bool late_service_rejected =
        false;

    try {
        host.queue(
            worker,
            "late"
        );
    } catch (
        const std::logic_error&
    ) {
        late_service_rejected = true;
    }

    assert(late_service_rejected);

    bool second_start_rejected =
        false;

    try {
        host.start();
    } catch (
        const std::logic_error&
    ) {
        second_start_rejected = true;
    }

    assert(second_start_rejected);

    const auto result =
        host.shutdown();

    assert(result.graceful);
    assert(result.pending.empty());
    assert(host.stopping());
    assert(host.token().cancelled());
    assert(!worker.running());
    assert(!scheduler.running());

    gungnir::Application
        failing_application;

    gungnir::scheduler::SystemClock
        failing_clock;

    gungnir::scheduler::Scheduler
        failing_scheduler{
            failing_clock
        };

    failing_scheduler.every(
        "fail-fast",
        1ms,
        [] {
            throw std::runtime_error(
                "scheduler failed"
            );
        }
    );

    gungnir::production::RuntimeHost
        failing_host{
            failing_application,
            options
        };

    gungnir::scheduler::RunnerOptions
        failing_options;

    failing_options.maximum_sleep =
        5s;

    failing_host.scheduler(
        failing_scheduler,
        failing_options,
        "scheduler-failing"
    );

    failing_host.start();

    for (
        int attempt = 0;
        attempt < 500 &&
        !failing_host.stopping();
        ++attempt
    ) {
        std::this_thread::sleep_for(
            1ms
        );
    }

    assert(failing_host.stopping());

    const auto failed_shutdown =
        failing_host.shutdown();

    assert(failed_shutdown.graceful);
    assert(
        failing_host.failure() !=
        nullptr
    );

    bool failure_rethrown = false;

    try {
        failing_host
            .rethrow_failure();
    } catch (
        const std::runtime_error&
            error
    ) {
        failure_rethrown =
            std::string{
                error.what()
            } ==
            "scheduler failed";
    }

    assert(failure_rethrown);

    return 0;
}
