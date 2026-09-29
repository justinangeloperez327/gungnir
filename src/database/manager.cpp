#include <gungnir/database/manager.hpp>

#include <atomic>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include <gungnir/database/connection.hpp>
#include <gungnir/database/transaction.hpp>

namespace gungnir::database {

class Manager::Impl {
public:
    mutable std::mutex mutex;

    std::unordered_map<
        String,
        std::shared_ptr<
            ConnectionPool
        >
    > primaries;

    std::unordered_map<
        String,
        std::vector<
            std::shared_ptr<
                ConnectionPool
            >
        >
    > replicas;

    std::unordered_map<
        String,
        std::size_t
    > replica_cursor;
};

namespace {

std::shared_ptr<ConnectionPool>
find_primary(
    Manager::Impl& impl,
    std::string_view name
) {
    const auto found =
        impl.primaries.find(
            String{name}
        );

    if (
        found ==
        impl.primaries.end()
    ) {
        throw std::out_of_range(
            "Gungnir database connection '" +
            String{name} +
            "' is not configured"
        );
    }

    return found->second;
}

} // namespace

Manager::Manager()
    : impl_(
        std::make_unique<Impl>()
      ) {}

Manager::~Manager() = default;

Manager::Manager(
    Manager&&
) noexcept = default;

Manager&
Manager::operator=(
    Manager&&
) noexcept = default;

Manager& Manager::add(
    String name,
    Backend backend,
    DriverFactory factory,
    std::size_t pool_size
) {
    return add(
        std::move(name),
        backend,
        std::move(factory),
        PoolOptions{
            .size = pool_size
        }
    );
}

Manager& Manager::add(
    String name,
    Backend backend,
    DriverFactory factory,
    PoolOptions options
) {
    auto pool =
        std::make_shared<
            ConnectionPool
        >(
            name,
            backend,
            std::move(factory),
            options
        );

    std::lock_guard lock{
        impl_->mutex
    };

    impl_->primaries
        .insert_or_assign(
            std::move(name),
            std::move(pool)
        );

    return *this;
}

Manager& Manager::add_replica(
    String primary,
    DriverFactory factory,
    PoolOptions options
) {
    std::lock_guard lock{
        impl_->mutex
    };

    auto write_pool =
        find_primary(
            *impl_,
            primary
        );

    auto replica =
        std::make_shared<
            ConnectionPool
        >(
            primary,
            write_pool->backend(),
            std::move(factory),
            options
        );

    impl_->replicas[
        primary
    ].push_back(
        std::move(replica)
    );

    return *this;
}

bool Manager::has(
    std::string_view name
) const noexcept {
    std::lock_guard lock{
        impl_->mutex
    };

    return
        impl_->primaries
            .contains(
                String{name}
            );
}

std::shared_ptr<Connection>
Manager::connection(
    std::string_view name,
    CancellationToken cancellation
) {
    return write_connection(
        name,
        std::move(cancellation)
    );
}

std::shared_ptr<Connection>
Manager::write_connection(
    std::string_view name,
    CancellationToken cancellation
) {
    std::shared_ptr<
        ConnectionPool
    > pool;

    {
        std::lock_guard lock{
            impl_->mutex
        };

        pool =
            find_primary(
                *impl_,
                name
            );
    }

    return pool->acquire(
        std::move(cancellation)
    );
}

std::shared_ptr<Connection>
Manager::read_connection(
    std::string_view name,
    CancellationToken cancellation
) {
    std::shared_ptr<
        ConnectionPool
    > pool;

    {
        std::lock_guard lock{
            impl_->mutex
        };

        const auto replica_found =
            impl_->replicas.find(
                String{name}
            );

        if (
            replica_found ==
                impl_->replicas.end() ||
            replica_found->
                second.empty()
        ) {
            pool =
                find_primary(
                    *impl_,
                    name
                );
        } else {
            auto& cursor =
                impl_->replica_cursor[
                    String{name}
                ];

            const auto index =
                cursor %
                replica_found->
                    second.size();

            cursor =
                (
                    cursor + 1
                ) %
                replica_found->
                    second.size();

            pool =
                replica_found->
                    second[index];
        }
    }

    return pool->acquire(
        std::move(cancellation)
    );
}

Backend Manager::backend(
    std::string_view name
) const {
    std::lock_guard lock{
        impl_->mutex
    };

    const auto found =
        impl_->primaries.find(
            String{name}
        );

    if (
        found ==
        impl_->primaries.end()
    ) {
        throw std::out_of_range(
            "Gungnir database connection '" +
            String{name} +
            "' is not configured"
        );
    }

    return
        found->second
            ->backend();
}

Transaction Manager::transaction(
    std::string_view name
) {
    return Transaction{
        write_connection(name)
    };
}

AsyncTransaction
Manager::async_transaction(
    std::string_view name
) {
    return AsyncTransaction{
        write_connection(name)
    };
}

void Manager::validate_pools() {
    std::vector<
        std::shared_ptr<
            ConnectionPool
        >
    > pools;

    {
        std::lock_guard lock{
            impl_->mutex
        };

        for (
            const auto& [
                name,
                pool
            ] :
            impl_->primaries
        ) {
            static_cast<void>(name);
            pools.push_back(pool);
        }

        for (
            const auto& [
                name,
                replicas
            ] :
            impl_->replicas
        ) {
            static_cast<void>(name);

            pools.insert(
                pools.end(),
                replicas.begin(),
                replicas.end()
            );
        }
    }

    for (auto& pool : pools) {
        pool->validate_idle();
    }
}

} // namespace gungnir::database
