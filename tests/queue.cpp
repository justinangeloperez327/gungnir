#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <gungnir/queue/queue.hpp>

int main() {
    using namespace gungnir;
    using namespace std::chrono_literals;

    queue::MemoryDriver driver;
    queue::Worker worker{driver};

    int handled = 0;

    worker.handle(
        "mail.send",
        [&](std::string_view payload) {
            assert(payload == "42");
            ++handled;
        }
    );

    driver.push({
        "1",
        "mail.send",
        "42",
        0,
        3
    });

    assert(worker.run_one());
    assert(handled == 1);
    assert(driver.pending() == 0);
    assert(driver.failed() == 0);

    worker.handle(
        "retry",
        [](
            std::string_view
        ) {
            throw std::runtime_error{
                "failure"
            };
        }
    );

    driver.push({
        "2",
        "retry",
        "",
        0,
        2
    });

    assert(worker.run_one());
    assert(driver.pending() == 1);
    assert(worker.run_one());
    assert(driver.failed() == 1);

    driver.push({
        "3",
        "unknown",
        "",
        0,
        1
    });

    assert(worker.run_one());
    assert(driver.failed() == 2);

    assert(!worker.run_one());

    queue::MemoryDriver delayed_driver;
    queue::Worker delayed_worker{
        delayed_driver
    };

    int delayed_handled = 0;

    delayed_worker.handle(
        "delayed",
        [&](
            std::string_view payload
        ) {
            assert(payload == "later");
            ++delayed_handled;
        }
    );

    delayed_driver.push_later(
        {
            "delayed-1",
            "delayed",
            "later",
            0,
            1
        },
        30ms
    );

    assert(
        delayed_driver.delayed() == 1
    );

    assert(
        !delayed_worker.run_one()
    );

    std::this_thread::sleep_for(
        40ms
    );

    assert(
        delayed_worker.run_one()
    );

    assert(delayed_handled == 1);
    assert(
        delayed_driver.delayed() == 0
    );

    queue::MemoryDriver lifecycle_driver;

    queue::WorkerOptions options;
    options.idle_sleep = 5ms;

    queue::Worker lifecycle{
        lifecycle_driver,
        options
    };

    std::mutex gate_mutex;
    std::condition_variable gate_ready;
    bool release_handler = false;
    std::atomic_bool handler_started{
        false
    };
    std::atomic_int lifecycle_handled{0};

    lifecycle.handle(
        "slow",
        [&](std::string_view payload) {
            assert(payload == "work");

            handler_started.store(
                true,
                std::memory_order_release
            );

            gate_ready.notify_all();

            std::unique_lock lock{
                gate_mutex
            };

            gate_ready.wait(
                lock,
                [&] {
                    return release_handler;
                }
            );

            lifecycle_handled.fetch_add(
                1,
                std::memory_order_relaxed
            );
        }
    );

    lifecycle_driver.push({
        "4",
        "slow",
        "work",
        0,
        1
    });

    std::size_t processed = 0;

    std::thread worker_thread{
        [&] {
            processed =
                lifecycle.run();
        }
    };

    {
        std::unique_lock lock{
            gate_mutex
        };

        gate_ready.wait(
            lock,
            [&] {
                return
                    handler_started.load(
                        std::memory_order_acquire
                    );
            }
        );

        lifecycle.request_stop();

        assert(
            lifecycle.stop_requested()
        );

        assert(
            lifecycle.running()
        );

        release_handler = true;
    }

    gate_ready.notify_all();
    worker_thread.join();

    assert(processed == 1);
    assert(lifecycle_handled.load() == 1);
    assert(lifecycle_driver.pending() == 0);
    assert(!lifecycle.running());

    lifecycle.reset_stop();

    CancellationSource stop_source;

    std::thread cancelled_worker{
        [&] {
            processed =
                lifecycle.run(
                    stop_source.token()
                );
        }
    };

    for (
        int attempt = 0;
        attempt < 100 &&
            !lifecycle.running();
        ++attempt
    ) {
        std::this_thread::sleep_for(
            1ms
        );
    }

    assert(lifecycle.running());

    stop_source.cancel();
    cancelled_worker.join();

    assert(processed == 0);
    assert(!lifecycle.running());

    return 0;
}
