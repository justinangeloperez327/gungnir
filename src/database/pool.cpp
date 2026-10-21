#include <gungnir/database/pool.hpp>

#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace gungnir::database {

class ConnectionPool::Impl {
public:
    struct Entry {
        std::unique_ptr<Connection> connection;
        bool leased{false};
        std::chrono::steady_clock::time_point
            validated_at{
                std::chrono::steady_clock::now()
            };
    };

    String name;
    Backend backend;
    DriverFactory factory;
    PoolOptions options;
    std::vector<Entry> entries;
    std::size_t next{0};
    std::size_t reconnects{0};
    std::size_t acquire_timeouts{0};
    std::mutex factory_mutex;
    mutable std::mutex mutex;
    std::condition_variable ready;
};

namespace {

// The entry remains exclusive while driver callbacks run without the pool
// mutex. Aliasing shared_ptrs keep this reservation alive for the full lease.
class EntryLease {
public:
    EntryLease(
        std::shared_ptr<ConnectionPool::Impl> impl,
        std::size_t index
    ) noexcept
        : impl_(std::move(impl)), index_(index) {}

    EntryLease(const EntryLease&) = delete;
    EntryLease& operator=(const EntryLease&) = delete;
    EntryLease(EntryLease&&) = delete;
    EntryLease& operator=(EntryLease&&) = delete;

    ~EntryLease() {
        {
            std::lock_guard lock{impl_->mutex};
            impl_->entries[index_].leased = false;
        }
        impl_->ready.notify_one();
    }

private:
    std::shared_ptr<ConnectionPool::Impl> impl_;
    std::size_t index_;
};

// The caller holds impl->mutex and has checked that the entry is available.
std::shared_ptr<EntryLease> reserve_entry(
    const std::shared_ptr<ConnectionPool::Impl>& impl,
    std::size_t index
) {
    // Allocate first so allocation failure cannot leave the entry reserved.
    auto lease = std::make_shared<EntryLease>(impl, index);
    impl->entries[index].leased = true;
    return lease;
}

void validate_options(
    const PoolOptions& options
) {
    if (options.size == 0) {
        throw std::invalid_argument(
            "Database pool size must be greater than zero"
        );
    }

    if (
        options.acquire_timeout.count() <= 0 ||
        options.validation_interval.count() <= 0
    ) {
        throw std::invalid_argument(
            "Database pool timeouts must be greater than zero"
        );
    }
}

std::unique_ptr<Connection>
make_connection(
    const std::shared_ptr<
        ConnectionPool::Impl
    >& impl
) {
    std::shared_ptr<Driver> driver;
    {
        // Preserve serialized factory calls without holding the mutex used
        // by pool waiters, lease returns, cancellation and statistics.
        std::lock_guard lock{impl->factory_mutex};
        driver = impl->factory();
    }

    if (!driver) {
        throw std::logic_error(
            "Gungnir driver factory returned null"
        );
    }

    if (
        driver->backend() !=
        impl->backend
    ) {
        throw std::logic_error(
            "Gungnir driver backend does not match connection backend"
        );
    }

    return
        std::make_unique<Connection>(
            impl->name,
            std::move(driver)
        );
}

bool should_validate(
    const ConnectionPool::Impl::Entry& entry,
    const PoolOptions& options,
    std::chrono::steady_clock::time_point now
) {
    return
        now - entry.validated_at >=
        options.validation_interval;
}

void ensure_healthy(
    const std::shared_ptr<
        ConnectionPool::Impl
    >& impl,
    std::size_t index
) {
    auto& entry =
        impl->entries[index];

    const auto now =
        std::chrono::steady_clock::now();

    if (
        !should_validate(
            entry,
            impl->options,
            now
        )
    ) {
        return;
    }

    bool healthy = false;

    try {
        healthy =
            entry.connection->healthy();
    } catch (...) {
        healthy = false;
    }

    if (healthy) {
        entry.validated_at = now;
        return;
    }

    std::exception_ptr last_error;

    for (
        std::size_t attempt = 0;
        attempt <=
            impl->options
                .reconnect_attempts;
        ++attempt
    ) {
        try {
            entry.connection =
                make_connection(impl);

            entry.validated_at =
                std::chrono::
                    steady_clock::now();

            {
                std::lock_guard lock{impl->mutex};
                ++impl->reconnects;
            }
            return;
        } catch (...) {
            last_error =
                std::current_exception();
        }
    }

    if (last_error) {
        std::rethrow_exception(
            last_error
        );
    }

    throw std::runtime_error(
        "Unable to reconnect database pool entry"
    );
}

} // namespace

ConnectionPool::ConnectionPool(
    String name,
    Backend backend,
    DriverFactory factory,
    std::size_t size
)
    : ConnectionPool(
        std::move(name),
        backend,
        std::move(factory),
        PoolOptions{
            .size = size
        }
      ) {}

ConnectionPool::ConnectionPool(
    String name,
    Backend backend,
    DriverFactory factory,
    PoolOptions options
)
    : impl_(
        std::make_shared<Impl>()
      ) {
    if (!factory) {
        throw std::invalid_argument(
            "Gungnir connection pool requires a driver factory"
        );
    }

    validate_options(options);

    impl_->name =
        std::move(name);

    impl_->backend = backend;
    impl_->factory =
        std::move(factory);

    impl_->options = options;
    impl_->entries.reserve(
        options.size
    );

    for (
        std::size_t index = 0;
        index < options.size;
        ++index
    ) {
        Impl::Entry entry;
        entry.connection =
            make_connection(impl_);

        impl_->entries.push_back(
            std::move(entry)
        );
    }
}

std::shared_ptr<Connection>
ConnectionPool::acquire(
    CancellationToken cancellation
) {
    const auto deadline =
        std::chrono::
            steady_clock::now() +
        impl_->options
            .acquire_timeout;

    auto wake =
        cancellation.on_cancel(
            [impl = impl_] {
                // Pair notification with the waiter's locked token check so
                // cancellation cannot be lost just before wait_until().
                std::lock_guard lock{impl->mutex};
                impl->ready.notify_all();
            }
        );

    std::unique_lock lock{
        impl_->mutex
    };

    while (true) {
        cancellation
            .throw_if_cancelled();

        for (
            std::size_t offset = 0;
            offset <
                impl_->entries.size();
            ++offset
        ) {
            const auto index =
                (
                    impl_->next +
                    offset
                ) %
                impl_->entries.size();

            auto& entry =
                impl_->entries[index];

            if (entry.leased) {
                continue;
            }

            auto lease = reserve_entry(impl_, index);
            impl_->next =
                (
                    index + 1
                ) %
                impl_->entries.size();

            lock.unlock();

            cancellation.throw_if_cancelled();
            ensure_healthy(impl_, index);
            cancellation.throw_if_cancelled();

            return std::shared_ptr<Connection>{
                std::move(lease), entry.connection.get()
            };
        }

        if (
            impl_->ready
                .wait_until(
                    lock,
                    deadline
                ) ==
            std::cv_status::timeout
        ) {
            // Cancellation wins over timeout when both become observable
            // at the acquisition deadline. This keeps cancellation
            // deterministic and avoids counting it as pool exhaustion.
            cancellation
                .throw_if_cancelled();

            ++impl_->
                acquire_timeouts;

            throw std::runtime_error(
                "Database pool acquisition timed out for '" +
                impl_->name +
                "'"
            );
        }
    }
}

const String&
ConnectionPool::name()
    const noexcept {
    return impl_->name;
}

Backend ConnectionPool::backend()
    const noexcept {
    return impl_->backend;
}

std::size_t ConnectionPool::size()
    const noexcept {
    return
        impl_->entries.size();
}

PoolStats ConnectionPool::stats()
    const noexcept {
    std::lock_guard lock{
        impl_->mutex
    };

    std::size_t leased = 0;

    for (
        const auto& entry :
        impl_->entries
    ) {
        if (entry.leased) {
            ++leased;
        }
    }

    return {
        .size =
            impl_->entries.size(),
        .leased = leased,
        .available =
            impl_->entries.size() -
            leased,
        .reconnects =
            impl_->reconnects,
        .acquire_timeouts =
            impl_->acquire_timeouts
    };
}

void ConnectionPool::validate_idle() {
    for (
        std::size_t index = 0;
        index <
            impl_->entries.size();
        ++index
    ) {
        std::shared_ptr<EntryLease> lease;
        {
            std::lock_guard lock{impl_->mutex};
            if (impl_->entries[index].leased) {
                continue;
            }
            lease = reserve_entry(impl_, index);
        }

        ensure_healthy(
            impl_,
            index
        );
    }
}

} // namespace gungnir::database
