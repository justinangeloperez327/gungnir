#include <atomic>
#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>

#include <gungnir/core/task.hpp>
#include <gungnir/database/database.hpp>

namespace {

struct AsyncTransactionWork {
    gungnir::Task<int>
    operator()() const {
        co_return 1;
    }
};

static_assert(
    !std::is_move_constructible_v<
        gungnir::database::Transaction
    >
);

static_assert(
    !std::is_move_assignable_v<
        gungnir::database::Transaction
    >
);

template <typename Work>
concept TransactionRunnable =
    requires(
        gungnir::database::Transaction& transaction,
        Work work
    ) {
        transaction.run(work);
    };

static_assert(
    !TransactionRunnable<
        AsyncTransactionWork
    >
);

} // namespace

int main() {
    using namespace gungnir;

    std::atomic_int next_driver{0};
    std::atomic_int began_on{-1};
    std::atomic_int transaction_query_on{-1};
    std::atomic_int outside_query_on{-1};
    std::atomic_int committed{0};

    std::mutex mutex;
    std::condition_variable ready;
    bool transaction_scope_ready = false;
    bool outside_complete = false;

    database::Manager manager;

    manager.add(
        "default",
        database::Backend::postgresql,
        [&] {
            const auto driver_id =
                next_driver.fetch_add(1);

            return std::make_shared<
                database::CallbackDriver
            >(
                database::Backend::postgresql,
                [&, driver_id](
                    const String& statement,
                    const auto&
                ) {
                    if (
                        statement ==
                        "TRANSACTION_QUERY"
                    ) {
                        transaction_query_on.store(
                            driver_id
                        );
                    }

                    if (
                        statement ==
                        "OUTSIDE_QUERY"
                    ) {
                        outside_query_on.store(
                            driver_id
                        );
                    }

                    return database::Result{};
                },
                [&, driver_id] {
                    began_on.store(
                        driver_id
                    );
                },
                [&] {
                    committed.fetch_add(1);
                }
            );
        },
        2
    );

    database::runtime::use(
        manager
    );

    assert(
        database::runtime::configured()
    );

    std::thread transaction_thread{
        [&] {
            auto transaction =
                manager.transaction();

            transaction.run(
                [&] {
                    {
                        std::lock_guard lock{
                            mutex
                        };

                        transaction_scope_ready =
                            true;
                    }

                    ready.notify_all();

                    {
                        std::unique_lock lock{
                            mutex
                        };

                        ready.wait(
                            lock,
                            [&] {
                                return
                                    outside_complete;
                            }
                        );
                    }

                    auto scoped =
                        database::runtime::
                            connection();

                    scoped->execute(
                        "TRANSACTION_QUERY"
                    );
                }
            );
        }
    };

    {
        std::unique_lock lock{
            mutex
        };

        ready.wait(
            lock,
            [&] {
                return
                    transaction_scope_ready;
            }
        );
    }

    assert(
        database::runtime::
            configured()
    );

    auto outside =
        database::runtime::
            connection();

    outside->execute(
        "OUTSIDE_QUERY"
    );

    {
        std::lock_guard lock{
            mutex
        };

        outside_complete = true;
    }

    ready.notify_all();

    transaction_thread.join();

    assert(
        began_on.load() >= 0
    );

    assert(
        transaction_query_on.load() ==
        began_on.load()
    );

    assert(
        outside_query_on.load() >= 0
    );

    assert(
        outside_query_on.load() !=
        began_on.load()
    );

    assert(
        committed.load() == 1
    );

    database::runtime::clear();

    assert(
        !database::runtime::
            configured()
    );

    return 0;
}
