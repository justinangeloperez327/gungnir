#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#include <gungnir/core/application.hpp>
#include <gungnir/production/production.hpp>

namespace {

using namespace std::chrono_literals;

template <typename Function>
bool throws_invalid_argument(
    Function&& function
) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }

    return false;
}

template <typename Function>
bool throws_logic_error(
    Function&& function
) {
    try {
        function();
    } catch (const std::logic_error&) {
        return true;
    }

    return false;
}

void health_contract() {
    using namespace gungnir::production;

    Health empty;

    assert(empty.live());
    assert(empty.ready());
    assert(
        empty.liveness_report()
            .checks.empty()
    );
    assert(
        empty.readiness_report()
            .checks.empty()
    );

    Health health;

    health
        .liveness(
            "process",
            [] {
                return true;
            }
        )
        .readiness(
            "database",
            [] {
                return true;
            }
        )
        .readiness(
            "queue",
            [] {
                return false;
            }
        )
        .readiness(
            "cache",
            []() -> bool {
                throw std::runtime_error(
                    "dependency unavailable"
                );
            }
        );

    assert(health.live());
    assert(!health.ready());

    const auto report =
        health.readiness_report();

    assert(!report.healthy);
    assert(report.checks.size() == 3);
    assert(report.checks[0].name == "database");
    assert(report.checks[0].healthy);
    assert(report.checks[1].name == "queue");
    assert(!report.checks[1].healthy);
    assert(report.checks[2].name == "cache");
    assert(!report.checks[2].healthy);

    assert(
        throws_invalid_argument(
            [&] {
                health.readiness(
                    "",
                    [] {
                        return true;
                    }
                );
            }
        )
    );

    assert(
        throws_logic_error(
            [&] {
                health.readiness(
                    "database",
                    [] {
                        return true;
                    }
                );
            }
        )
    );
}

void retry_contract() {
    using namespace gungnir;
    using namespace gungnir::production;

    RetryOptions options;
    options.max_attempts = 3;
    options.initial_backoff = 0ms;
    options.max_backoff = 0ms;
    options.multiplier = 2.0;

    int attempts = 0;

    const auto value =
        retry(
            options,
            [&]() -> int {
                ++attempts;

                if (attempts < 3) {
                    throw std::runtime_error(
                        "transient"
                    );
                }

                return 42;
            },
            [](
                const std::exception_ptr&,
                std::size_t
            ) {
                return true;
            }
        );

    assert(value == 42);
    assert(attempts == 3);

    attempts = 0;
    bool permanent_rethrown = false;

    try {
        static_cast<void>(
            retry(
                options,
                [&]() -> int {
                    ++attempts;
                    throw std::logic_error(
                        "permanent"
                    );
                },
                [](
                    const std::exception_ptr&,
                    std::size_t
                ) {
                    return false;
                }
            )
        );
    } catch (const std::logic_error&) {
        permanent_rethrown = true;
    }

    assert(permanent_rethrown);
    assert(attempts == 1);

    CancellationSource source;
    source.cancel();

    bool cancelled = false;

    try {
        static_cast<void>(
            retry(
                options,
                [] {
                    return 1;
                },
                [](
                    const std::exception_ptr&,
                    std::size_t
                ) {
                    return true;
                },
                source.token()
            )
        );
    } catch (const OperationCancelled&) {
        cancelled = true;
    }

    assert(cancelled);

    assert(
        throws_invalid_argument(
            [] {
                RetryOptions invalid;
                invalid.max_attempts = 0;

                static_cast<void>(
                    retry(
                        invalid,
                        [] {
                            return 1;
                        },
                        [](
                            const std::exception_ptr&,
                            std::size_t
                        ) {
                            return true;
                        }
                    )
                );
            }
        )
    );
}

void runtime_host_health_contract() {
    using namespace gungnir;

    Application application;

    production::RuntimeHost host{
        application
    };

    assert(host.health().live());
    assert(!host.health().ready());

    host.start();

    assert(application.is_booted());
    assert(host.health().live());
    assert(host.health().ready());

    host.request_stop();

    assert(host.stopping());
    assert(!host.health().ready());

    const auto result =
        host.shutdown();

    assert(result.graceful);
}

} // namespace

int main() {
    health_contract();
    retry_contract();
    runtime_host_health_contract();
}
