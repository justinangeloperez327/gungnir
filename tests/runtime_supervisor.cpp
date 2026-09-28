#include <atomic>
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#include <gungnir/production/production.hpp>

int main() {
    using namespace std::chrono_literals;
    using gungnir::production::Supervisor;
    using gungnir::production::SupervisorOptions;

    SupervisorOptions options;
    options.shutdown_timeout = 1s;
    options.poll_interval = 2ms;

    Supervisor supervisor{
        options
    };

    std::atomic_bool
        first_running{true};

    std::atomic_bool
        second_running{true};

    std::atomic_int
        first_stop_calls{0};

    std::atomic_int
        second_stop_calls{0};

    std::atomic_bool
        token_cancelled{false};

    auto registration =
        supervisor
            .token()
            .on_cancel(
                [&] {
                    token_cancelled.store(
                        true,
                        std::memory_order_release
                    );
                }
            );

    supervisor
        .runtime(
            "http",
            [&] {
                ++first_stop_calls;

                std::thread{
                    [&] {
                        std::this_thread::
                            sleep_for(20ms);

                        first_running.store(
                            false,
                            std::memory_order_release
                        );

                        supervisor.notify();
                    }
                }.detach();
            },
            [&] {
                return
                    first_running.load(
                        std::memory_order_acquire
                    );
            }
        )
        .runtime(
            "queue",
            [&] {
                ++second_stop_calls;

                second_running.store(
                    false,
                    std::memory_order_release
                );

                supervisor.notify();
            },
            [&] {
                return
                    second_running.load(
                        std::memory_order_acquire
                    );
            }
        );

    assert(supervisor.size() == 2);
    assert(!supervisor.stopping());

    const auto result =
        supervisor.shutdown();

    assert(result.graceful);
    assert(result.pending.empty());
    assert(supervisor.stopping());

    assert(
        token_cancelled.load(
            std::memory_order_acquire
        )
    );

    assert(first_stop_calls == 1);
    assert(second_stop_calls == 1);

    supervisor.request_stop();

    assert(first_stop_calls == 1);
    assert(second_stop_calls == 1);

    bool late_registration_rejected =
        false;

    try {
        supervisor.runtime(
            "late",
            [] {},
            [] {
                return false;
            }
        );
    } catch (
        const std::logic_error&
    ) {
        late_registration_rejected =
            true;
    }

    assert(late_registration_rejected);

    Supervisor duplicate;

    duplicate.runtime(
        "worker",
        [] {},
        [] {
            return false;
        }
    );

    bool duplicate_rejected = false;

    try {
        duplicate.runtime(
            "worker",
            [] {},
            [] {
                return false;
            }
        );
    } catch (
        const std::logic_error&
    ) {
        duplicate_rejected = true;
    }

    assert(duplicate_rejected);

    SupervisorOptions timeout_options;
    timeout_options.shutdown_timeout =
        35ms;
    timeout_options.poll_interval =
        2ms;

    Supervisor timeout{
        timeout_options
    };

    std::atomic_int
        failed_stop_calls{0};

    timeout.runtime(
        "stuck",
        [&] {
            ++failed_stop_calls;

            throw std::runtime_error(
                "stop failure"
            );
        },
        [] {
            return true;
        }
    );

    const auto timeout_started =
        std::chrono::
            steady_clock::now();

    const auto timed_out =
        timeout.shutdown();

    const auto timeout_elapsed =
        std::chrono::
            steady_clock::now() -
        timeout_started;

    assert(!timed_out.graceful);
    assert(
        timed_out.pending ==
        std::vector<std::string>{
            "stuck"
        }
    );

    assert(failed_stop_calls == 1);

    assert(
        timeout_elapsed >= 25ms
    );

    assert(
        timeout_elapsed < 1s
    );

    Supervisor empty;

    const auto empty_result =
        empty.shutdown();

    assert(empty_result.graceful);
    assert(empty_result.pending.empty());

    return 0;
}
