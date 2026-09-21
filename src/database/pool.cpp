#include <gungnir/database/pool.hpp>

#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace gungnir::database {

class ConnectionPool::Impl {
public:
    String name;
    Backend backend;
    std::vector<std::shared_ptr<Connection>> connections;
    std::size_t next{0};
    std::mutex mutex;
};

ConnectionPool::ConnectionPool(
    String name,
    Backend backend,
    DriverFactory factory,
    std::size_t size
)
    : impl_(std::make_shared<Impl>()) {
    if (!factory) {
        throw std::invalid_argument(
            "Gungnir connection pool requires a driver factory"
        );
    }

    if (size == 0) {
        throw std::invalid_argument(
            "Gungnir connection pool size must be greater than zero"
        );
    }

    impl_->name = std::move(name);
    impl_->backend = backend;
    impl_->connections.reserve(size);

    for (std::size_t index = 0; index < size; ++index) {
        auto driver = factory();

        if (!driver) {
            throw std::logic_error(
                "Gungnir driver factory returned null"
            );
        }

        if (driver->backend() != backend) {
            throw std::logic_error(
                "Gungnir driver backend does not match connection backend"
            );
        }

        impl_->connections.push_back(
            std::make_shared<Connection>(
                impl_->name,
                std::move(driver)
            )
        );
    }
}

std::shared_ptr<Connection> ConnectionPool::acquire() {
    std::lock_guard lock{impl_->mutex};

    auto connection = impl_->connections[impl_->next];
    impl_->next = (impl_->next + 1) % impl_->connections.size();

    return connection;
}

const String& ConnectionPool::name() const noexcept {
    return impl_->name;
}

Backend ConnectionPool::backend() const noexcept {
    return impl_->backend;
}

std::size_t ConnectionPool::size() const noexcept {
    return impl_->connections.size();
}

} // namespace gungnir::database
