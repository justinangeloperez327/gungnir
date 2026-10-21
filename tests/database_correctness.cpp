#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/database/database.hpp>

namespace {

using namespace std::chrono_literals;

class RecordingDriver final : public gungnir::database::Driver {
public:
    explicit RecordingDriver(
        bool healthy = true
    )
        : healthy_(healthy) {}

    [[nodiscard]]
    gungnir::database::Backend backend()
        const noexcept override {
        return gungnir::database::Backend::postgresql;
    }

    gungnir::database::Result execute(
        const gungnir::String& statement,
        const std::vector<
            gungnir::model::AttributeValue
        >& = {}
    ) override {
        std::lock_guard lock{mutex_};
        statements_.push_back(statement);
        return {};
    }

    void begin() override {
        ++begins_;
    }

    void commit() override {
        ++commits_;
    }

    void rollback() override {
        ++rollbacks_;
    }

    [[nodiscard]]
    bool ping() override {
        return healthy_;
    }

    [[nodiscard]]
    bool supports_savepoints()
        const noexcept override {
        return true;
    }

    [[nodiscard]]
    int begins() const noexcept {
        return begins_.load();
    }

    [[nodiscard]]
    int commits() const noexcept {
        return commits_.load();
    }

    [[nodiscard]]
    int rollbacks() const noexcept {
        return rollbacks_.load();
    }

    [[nodiscard]]
    std::vector<gungnir::String>
    statements() const {
        std::lock_guard lock{mutex_};
        return statements_;
    }

private:
    bool healthy_{true};
    std::atomic_int begins_{0};
    std::atomic_int commits_{0};
    std::atomic_int rollbacks_{0};
    mutable std::mutex mutex_;
    std::vector<gungnir::String> statements_;
};

void transaction_scopes_are_strictly_lifo() {
    using namespace gungnir;
    using namespace gungnir::database;

    auto driver =
        std::make_shared<RecordingDriver>();

    auto connection =
        std::make_shared<Connection>(
            "default",
            driver
        );

    Transaction outer{connection};
    Transaction inner{connection};

    bool rejected = false;

    try {
        outer.commit();
    } catch (const std::logic_error&) {
        rejected = true;
    }

    assert(rejected);
    assert(outer.active());
    assert(inner.active());
    assert(connection->in_transaction());
    assert(driver->commits() == 0);

    int after_commit = 0;

    connection->after_commit(
        [&] {
            ++after_commit;
        }
    );

    inner.commit();

    assert(!inner.active());
    assert(outer.active());
    assert(connection->in_transaction());
    assert(after_commit == 0);

    outer.commit();

    assert(!outer.active());
    assert(!connection->in_transaction());
    assert(driver->commits() == 1);
    assert(after_commit == 1);

    const auto statements =
        driver->statements();

    assert(statements.size() == 2);
    assert(
        statements[0].starts_with(
            "SAVEPOINT gungnir_sp_"
        )
    );
    assert(
        statements[1].starts_with(
            "RELEASE SAVEPOINT gungnir_sp_"
        )
    );
}

void rolled_back_nested_callbacks_do_not_escape() {
    using namespace gungnir;
    using namespace gungnir::database;

    auto driver =
        std::make_shared<RecordingDriver>();

    auto connection =
        std::make_shared<Connection>(
            "default",
            driver
        );

    int callback_count = 0;

    Transaction outer{connection};

    {
        Transaction inner{connection};

        connection->after_commit(
            [&] {
                ++callback_count;
            }
        );

        inner.rollback();
        assert(!inner.active());
    }

    outer.commit();

    assert(callback_count == 0);
    assert(driver->commits() == 1);
    assert(!connection->in_transaction());

    const auto statements =
        driver->statements();

    assert(statements.size() == 2);
    assert(
        statements[0].starts_with(
            "SAVEPOINT gungnir_sp_"
        )
    );
    assert(
        statements[1].starts_with(
            "ROLLBACK TO SAVEPOINT gungnir_sp_"
        )
    );
}

void commit_callback_failure_does_not_reactivate_transaction() {
    using namespace gungnir;
    using namespace gungnir::database;

    auto driver =
        std::make_shared<RecordingDriver>();

    auto connection =
        std::make_shared<Connection>(
            "default",
            driver
        );

    Transaction transaction{connection};

    connection->after_commit(
        [] {
            throw std::runtime_error(
                "after-commit failure"
            );
        }
    );

    bool failed = false;

    try {
        transaction.commit();
    } catch (const std::runtime_error&) {
        failed = true;
    }

    assert(failed);
    assert(!transaction.active());
    assert(!connection->in_transaction());
    assert(driver->commits() == 1);
    assert(driver->rollbacks() == 0);
}

void pool_cancellation_does_not_count_as_timeout() {
    using namespace gungnir;
    using namespace gungnir::database;

    Manager manager;

    manager.add(
        "default",
        Backend::postgresql,
        [] {
            return
                std::make_shared<
                    RecordingDriver
                >();
        },
        PoolOptions{
            .size = 1,
            .acquire_timeout = 500ms,
            .validation_interval = 30s,
            .reconnect_attempts = 1
        }
    );

    auto held =
        manager.connection();

    auto stats =
        manager.pool_stats();

    assert(stats.leased == 1);
    assert(stats.available == 0);
    assert(stats.acquire_timeouts == 0);

    CancellationSource cancellation;
    std::atomic_bool cancelled{false};

    std::thread waiter{
        [&] {
            try {
                static_cast<void>(
                    manager.connection(
                        "default",
                        cancellation.token()
                    )
                );
            } catch (
                const OperationCancelled&
            ) {
                cancelled.store(
                    true,
                    std::memory_order_release
                );
            }
        }
    };

    std::this_thread::sleep_for(30ms);
    cancellation.cancel();
    waiter.join();

    assert(
        cancelled.load(
            std::memory_order_acquire
        )
    );

    stats = manager.pool_stats();

    assert(stats.leased == 1);
    assert(stats.acquire_timeouts == 0);

    held.reset();

    stats = manager.pool_stats();

    assert(stats.leased == 0);
    assert(stats.available == 1);
}

void pool_timeout_and_reconnect_are_accounted() {
    using namespace gungnir;
    using namespace gungnir::database;

    {
        Manager manager;

        manager.add(
            "default",
            Backend::postgresql,
            [] {
                return
                    std::make_shared<
                        RecordingDriver
                    >();
            },
            PoolOptions{
                .size = 1,
                .acquire_timeout = 40ms,
                .validation_interval = 30s,
                .reconnect_attempts = 1
            }
        );

        auto held =
            manager.connection();

        bool timed_out = false;

        try {
            static_cast<void>(
                manager.connection()
            );
        } catch (
            const std::runtime_error&
        ) {
            timed_out = true;
        }

        assert(timed_out);

        auto stats =
            manager.pool_stats();

        assert(stats.leased == 1);
        assert(stats.acquire_timeouts == 1);

        held.reset();

        stats = manager.pool_stats();

        assert(stats.leased == 0);
        assert(stats.available == 1);
    }

    {
        std::atomic_int generation{0};

        Manager manager;

        manager.add(
            "default",
            Backend::postgresql,
            [&] {
                const auto id =
                    generation.fetch_add(1);

                return
                    std::make_shared<
                        RecordingDriver
                    >(
                        id != 0
                    );
            },
            PoolOptions{
                .size = 1,
                .acquire_timeout = 200ms,
                .validation_interval = 1ms,
                .reconnect_attempts = 1
            }
        );

        std::this_thread::sleep_for(5ms);

        {
            auto connection =
                manager.connection();

            assert(connection->healthy());
        }

        const auto stats =
            manager.pool_stats();

        assert(stats.reconnects == 1);
        assert(stats.leased == 0);
        assert(stats.available == 1);
        assert(generation.load() == 2);
    }
}

class ValidationGate {
public:
    void block() {
        std::unique_lock lock{mutex_};
        entered_ = true;
        changed_.notify_all();
        // Bound a broken regression's lifetime even if its caller cannot
        // release the gate until another pool operation has completed.
        changed_.wait_for(lock, 5s, [&] { return released_; });
    }

    bool wait_until_entered() {
        std::unique_lock lock{mutex_};
        return changed_.wait_for(lock, 2s, [&] { return entered_; });
    }

    void release() {
        {
            std::lock_guard lock{mutex_};
            released_ = true;
        }
        changed_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable changed_;
    bool entered_{false};
    bool released_{false};
};

std::shared_ptr<gungnir::database::CallbackDriver> pool_driver(
    gungnir::database::CallbackDriver::Ping ping
) {
    using namespace gungnir::database;
    return std::make_shared<CallbackDriver>(
        Backend::postgresql,
        [](const gungnir::String&, const auto&) { return Result{}; },
        CallbackDriver::Action{}, CallbackDriver::Action{},
        CallbackDriver::Action{}, std::move(ping)
    );
}

void pool_validation_does_not_block_other_entries() {
    using namespace gungnir::database;

    for (const auto [idle_validation, reconnecting] : {
        std::pair{false, false}, std::pair{true, false},
        std::pair{false, true}, std::pair{true, true}
    }) {
        ValidationGate gate;
        std::atomic_bool checking{false};
        std::atomic_int generation{0};
        ConnectionPool pool{
            "default", Backend::postgresql,
            [&] {
                const int id = generation.fetch_add(1);
                if (reconnecting && checking.load() && id >= 2) gate.block();
                return pool_driver([&, id] {
                    if (id == 0 && checking.load()) {
                        if (!reconnecting) gate.block();
                        return !reconnecting;
                    }
                    return true;
                });
            },
            PoolOptions{.size = 2, .acquire_timeout = 100ms,
                        .validation_interval = 1ms}
        };

        auto first = pool.acquire();
        auto held = pool.acquire();
        first.reset();
        std::this_thread::sleep_for(5ms);
        checking.store(true);

        auto validator = std::async(std::launch::async, [&] {
            if (idle_validation) {
                pool.validate_idle();
                return std::shared_ptr<Connection>{};
            }
            return pool.acquire();
        });
        const bool entered = gate.wait_until_entered();

        auto returned = std::async(std::launch::async,
            [lease = std::move(held)]() mutable { lease.reset(); });
        const bool returned_while_checking =
            returned.wait_for(1s) == std::future_status::ready;
        auto borrower = std::async(std::launch::async,
            [&] { return pool.acquire(); });
        const bool acquired_while_checking =
            borrower.wait_for(1s) == std::future_status::ready;
        auto statistics = std::async(std::launch::async,
            [&] { return pool.stats(); });
        const bool stats_while_checking =
            statistics.wait_for(1s) == std::future_status::ready;

        gate.release();
        returned.get();
        auto validated = validator.get();
        auto borrowed = borrower.get();
        const auto stats = statistics.get();
        assert(entered);
        assert(returned_while_checking);
        assert(acquired_while_checking);
        assert(stats_while_checking);
        assert(stats.leased == 2);
        assert(stats.available == 0);
        assert(!validated || validated != borrowed);
        assert(pool.stats().reconnects == (reconnecting ? 1U : 0U));

        auto retained = borrowed;
        borrowed.reset();
        validated.reset();
        assert(pool.stats().leased == 1);
        retained.reset();
        assert(pool.stats().available == 2);
    }
}

void pool_waiters_remain_cancellable_during_validation() {
    using namespace gungnir;
    using namespace gungnir::database;

    ValidationGate gate;
    ConnectionPool pool{
        "default", Backend::postgresql,
        [&] { return pool_driver([&] { gate.block(); return true; }); },
        PoolOptions{.size = 1, .acquire_timeout = 250ms,
                    .validation_interval = 1ms}
    };
    std::this_thread::sleep_for(5ms);
    auto validator = std::async(std::launch::async,
        [&] { return pool.acquire(); });
    const bool entered = gate.wait_until_entered();

    CancellationSource cancellation;
    std::promise<void> waiter_started;
    auto started = waiter_started.get_future();
    auto waiter = std::async(std::launch::async, [&] {
        waiter_started.set_value();
        try {
            (void)pool.acquire(cancellation.token());
        } catch (const OperationCancelled&) {
            return true;
        }
        return false;
    });
    started.wait();
    const bool initially_waiting =
        waiter.wait_for(20ms) == std::future_status::timeout;
    cancellation.cancel();
    const bool cancelled_while_checking =
        waiter.wait_for(1s) == std::future_status::ready;

    auto exhausted = std::async(std::launch::async, [&] {
        try {
            (void)pool.acquire();
        } catch (const std::runtime_error& error) {
            return std::string{error.what()}.find("timed out") != std::string::npos;
        }
        return false;
    });
    const bool timed_out_while_checking =
        exhausted.wait_for(1s) == std::future_status::ready;

    gate.release();
    auto validated = validator.get();
    const bool cancelled = waiter.get();
    const bool timed_out = exhausted.get();
    assert(entered);
    assert(initially_waiting);
    assert(cancelled_while_checking && cancelled);
    assert(timed_out_while_checking && timed_out);
    assert(pool.stats().leased == 1);
    assert(pool.stats().acquire_timeouts == 1);
    validated.reset();
    assert(pool.stats().available == 1);
}

void pool_cancellation_after_validation_releases_reservation() {
    using namespace gungnir;
    using namespace gungnir::database;

    ValidationGate gate;
    ConnectionPool pool{
        "default", Backend::postgresql,
        [&] { return pool_driver([&] { gate.block(); return true; }); },
        PoolOptions{.size = 1, .validation_interval = 1ms}
    };
    std::this_thread::sleep_for(5ms);
    CancellationSource cancellation;
    auto validator = std::async(std::launch::async, [&] {
        try {
            (void)pool.acquire(cancellation.token());
        } catch (const OperationCancelled&) {
            return true;
        }
        return false;
    });
    const bool entered = gate.wait_until_entered();
    cancellation.cancel();
    gate.release();
    const bool cancelled = validator.get();
    assert(entered && cancelled);
    assert(pool.stats().available == 1);
    assert(pool.stats().acquire_timeouts == 0);
    auto recovered = pool.acquire();
    assert(recovered);
}

void pool_validation_failure_releases_reservation() {
    using namespace gungnir::database;

    for (const bool idle_validation : {false, true}) {
        std::atomic_bool reject_factory{false};
        std::atomic_int generation{0};
        ConnectionPool pool{
            "default", Backend::postgresql,
            [&] {
                if (reject_factory.load()) throw std::runtime_error("Reconnect failed");
                const int id = generation.fetch_add(1);
                return pool_driver([id] { return id != 0; });
            },
            PoolOptions{.size = 1, .validation_interval = 1ms,
                        .reconnect_attempts = 0}
        };
        std::this_thread::sleep_for(5ms);
        reject_factory.store(true);
        bool failed = false;
        try {
            if (idle_validation) pool.validate_idle();
            else (void)pool.acquire();
        } catch (const std::runtime_error&) {
            failed = true;
        }
        assert(failed);
        assert(pool.stats().available == 1);
        assert(pool.stats().reconnects == 0);

        reject_factory.store(false);
        auto recovered = pool.acquire();
        assert(recovered->healthy());
        assert(pool.stats().reconnects == 1);
        recovered.reset();
        assert(pool.stats().available == 1);
    }
}

void database_errors_preserve_context() {
    using namespace gungnir;
    using namespace gungnir::database;

    auto driver =
        std::make_shared<CallbackDriver>(
            Backend::postgresql,
            [](
                const String&,
                const std::vector<
                    model::AttributeValue
                >&
            ) -> Result {
                throw std::runtime_error(
                    "driver failure"
                );
            }
        );

    Connection connection{
        "analytics",
        driver
    };

    bool failed = false;

    try {
        static_cast<void>(
            connection.execute(
                "SELECT broken"
            )
        );
    } catch (const Error& error) {
        failed = true;
        assert(
            error.backend() ==
            Backend::postgresql
        );
        assert(
            error.connection() ==
            "analytics"
        );
        assert(
            error.statement() ==
            "SELECT broken"
        );
    }

    assert(failed);
}

} // namespace

int main() {
    transaction_scopes_are_strictly_lifo();
    rolled_back_nested_callbacks_do_not_escape();
    commit_callback_failure_does_not_reactivate_transaction();
    pool_cancellation_does_not_count_as_timeout();
    pool_timeout_and_reconnect_are_accounted();
    pool_validation_does_not_block_other_entries();
    pool_waiters_remain_cancellable_during_validation();
    pool_cancellation_after_validation_releases_reservation();
    pool_validation_failure_releases_reservation();
    database_errors_preserve_context();
}
