#include <atomic>
#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/database/connection.hpp>
#include <gungnir/database/driver.hpp>

int main() {
    using namespace gungnir;
    using namespace gungnir::database;

    std::atomic_bool executed{false};
    std::atomic_bool cancelled_hook{false};
    std::atomic_bool observed_cancel{false};

    std::mutex mutex;
    std::condition_variable ready;
    bool started = false;
    bool release = false;

    auto driver =
        std::make_shared<
            CallbackDriver
        >(
            Backend::postgresql,
            [&](
                const String&,
                const std::vector<
                    model::AttributeValue
                >&
            ) {
                executed.store(true);

                std::unique_lock lock{
                    mutex
                };

                started = true;
                ready.notify_all();

                ready.wait(
                    lock,
                    [&] {
                        return release;
                    }
                );

                return Result{};
            },
            CallbackDriver::Action{},
            CallbackDriver::Action{},
            CallbackDriver::Action{},
            CallbackDriver::Ping{},
            [&] {
                cancelled_hook.store(true);

                {
                    std::lock_guard lock{
                        mutex
                    };

                    release = true;
                }

                ready.notify_all();
            }
        );

    Connection connection{
        "default",
        driver
    };

    CancellationSource pre_cancelled;
    pre_cancelled.cancel();

    bool rejected_before_execute = false;

    try {
        static_cast<void>(
            connection.execute(
                "SELECT 1",
                {},
                pre_cancelled.token()
            )
        );
    } catch (
        const OperationCancelled&
    ) {
        rejected_before_execute = true;
    }

    assert(
        rejected_before_execute
    );
    assert(!executed.load());

    CancellationSource source;

    std::thread worker{
        [&] {
            try {
                static_cast<void>(
                    connection.execute(
                        "SELECT pg_sleep(30)",
                        {},
                        source.token()
                    )
                );
            } catch (
                const OperationCancelled&
            ) {
                observed_cancel.store(
                    true
                );
            }
        }
    };

    {
        std::unique_lock lock{
            mutex
        };

        ready.wait(
            lock,
            [&] {
                return started;
            }
        );
    }

    source.cancel();

    worker.join();

    assert(cancelled_hook.load());
    assert(observed_cancel.load());

    std::atomic_int late_cancel_calls{0};

    auto fast_driver =
        std::make_shared<
            CallbackDriver
        >(
            Backend::postgresql,
            [](
                const String&,
                const std::vector<
                    model::AttributeValue
                >&
            ) {
                return Result{};
            },
            CallbackDriver::Action{},
            CallbackDriver::Action{},
            CallbackDriver::Action{},
            CallbackDriver::Ping{},
            [&] {
                late_cancel_calls
                    .fetch_add(1);
            }
        );

    Connection fast{
        "fast",
        fast_driver
    };

    CancellationSource completed;

    static_cast<void>(
        fast.execute(
            "SELECT 1",
            {},
            completed.token()
        )
    );

    completed.cancel();

    assert(
        late_cancel_calls.load() ==
        0
    );

    return 0;
}
