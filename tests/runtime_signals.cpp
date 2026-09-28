#include <atomic>
#include <cassert>
#include <chrono>
#include <csignal>
#include <thread>

#include <gungnir/core/application.hpp>
#include <gungnir/production/runtime_host.hpp>
#include <gungnir/production/signal_watcher.hpp>
#include <gungnir/queue/memory_driver.hpp>
#include <gungnir/queue/worker.hpp>

int main() {
    using namespace std::chrono_literals;

    std::atomic_int
        first_signal{0};

    gungnir::production::SignalWatcher
        first{
            [&](int signal_number) {
                first_signal.store(
                    signal_number,
                    std::memory_order_release
                );
            },
            1ms
        };

    for (
        int attempt = 0;
        attempt < 500 &&
        !first.running();
        ++attempt
    ) {
        std::this_thread::sleep_for(
            1ms
        );
    }

    assert(first.running());

    first.stop();

    assert(!first.running());

    std::atomic_int
        second_signal{0};

    gungnir::production::SignalWatcher
        second{
            [&](int signal_number) {
                second_signal.store(
                    signal_number,
                    std::memory_order_release
                );
            },
            1ms
        };

    for (
        int attempt = 0;
        attempt < 500 &&
        !second.running();
        ++attempt
    ) {
        std::this_thread::sleep_for(
            1ms
        );
    }

    assert(second.running());

    // Re-stopping an older watcher must not restore handlers
    // owned by the active watcher.
    first.stop();

    assert(
        std::raise(SIGTERM) == 0
    );

    for (
        int attempt = 0;
        attempt < 500 &&
        second_signal.load(
            std::memory_order_acquire
        ) == 0;
        ++attempt
    ) {
        std::this_thread::sleep_for(
            1ms
        );
    }

    assert(
        second_signal.load(
            std::memory_order_acquire
        ) ==
        SIGTERM
    );

    second.stop();

    gungnir::production::
        SupervisorOptions options;

    options.shutdown_timeout = 1s;
    options.poll_interval = 2ms;

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

    gungnir::production::RuntimeHost host{
        application,
        options
    };

    host.queue(
        worker,
        "queue"
    );

    std::atomic_int
        host_signal{0};

    gungnir::production::SignalWatcher
        host_watcher{
            [&](int signal_number) {
                host_signal.store(
                    signal_number,
                    std::memory_order_release
                );

                host.request_stop();
            },
            1ms
        };

    host.start();

    for (
        int attempt = 0;
        attempt < 500 &&
        (
            !worker.running() ||
            !host_watcher.running()
        );
        ++attempt
    ) {
        std::this_thread::sleep_for(
            1ms
        );
    }

    assert(worker.running());
    assert(host_watcher.running());

    assert(
        std::raise(SIGINT) == 0
    );

    for (
        int attempt = 0;
        attempt < 500 &&
        !host.stopping();
        ++attempt
    ) {
        std::this_thread::sleep_for(
            1ms
        );
    }

    assert(host.stopping());

    const auto result =
        host.shutdown();

    assert(result.graceful);
    assert(result.pending.empty());
    assert(!worker.running());

    assert(
        host_signal.load(
            std::memory_order_acquire
        ) ==
        SIGINT
    );

    assert(
        host_watcher.last_signal() ==
        SIGINT
    );

    host_watcher.stop();

    return 0;
}
