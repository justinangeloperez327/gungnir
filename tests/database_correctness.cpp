#include <atomic>
#include <cassert>
#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
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
    database_errors_preserve_context();
}
