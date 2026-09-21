#include <gungnir/database/manager.hpp>

#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <gungnir/database/connection.hpp>
#include <gungnir/database/pool.hpp>
#include <gungnir/database/transaction.hpp>

namespace gungnir::database {

class Manager::Impl {
public:
    mutable std::mutex mutex;
    std::unordered_map<String, std::shared_ptr<ConnectionPool>> pools;
};

Manager::Manager()
    : impl_(std::make_unique<Impl>()) {}

Manager::~Manager() = default;
Manager::Manager(Manager&&) noexcept = default;
Manager& Manager::operator=(Manager&&) noexcept = default;

Manager& Manager::add(
    String name,
    Backend backend,
    DriverFactory factory,
    std::size_t pool_size
) {
    auto pool = std::make_shared<ConnectionPool>(
        name,
        backend,
        std::move(factory),
        pool_size
    );

    std::lock_guard lock{impl_->mutex};
    impl_->pools.insert_or_assign(
        std::move(name),
        std::move(pool)
    );

    return *this;
}

bool Manager::has(std::string_view name) const noexcept {
    std::lock_guard lock{impl_->mutex};
    return impl_->pools.contains(String{name});
}

std::shared_ptr<Connection> Manager::connection(
    std::string_view name
) {
    std::shared_ptr<ConnectionPool> pool;

    {
        std::lock_guard lock{impl_->mutex};

        const auto found = impl_->pools.find(String{name});
        if (found == impl_->pools.end()) {
            throw std::out_of_range(
                "Gungnir database connection '" +
                String{name} +
                "' is not configured"
            );
        }

        pool = found->second;
    }

    return pool->acquire();
}

Backend Manager::backend(std::string_view name) const {
    std::lock_guard lock{impl_->mutex};

    const auto found = impl_->pools.find(String{name});
    if (found == impl_->pools.end()) {
        throw std::out_of_range(
            "Gungnir database connection '" +
            String{name} +
            "' is not configured"
        );
    }

    return found->second->backend();
}

Transaction Manager::transaction(std::string_view name) {
    return Transaction{connection(name)};
}

} // namespace gungnir::database
