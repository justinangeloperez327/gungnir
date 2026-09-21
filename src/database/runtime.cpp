#include <gungnir/database/runtime.hpp>

#include <stdexcept>
#include <utility>

namespace gungnir::database::runtime {

namespace {
Manager* active_manager = nullptr;
thread_local std::shared_ptr<Connection> scoped_connection;
}

ConnectionScope::ConnectionScope(
    std::shared_ptr<Connection> connection
)
    : previous_(std::move(scoped_connection)) {
    if (!connection) {
        throw std::invalid_argument(
            "Database connection scope requires a connection"
        );
    }

    scoped_connection = std::move(connection);
}

ConnectionScope::~ConnectionScope() {
    if (active_) {
        scoped_connection = std::move(previous_);
    }
}

ConnectionScope::ConnectionScope(ConnectionScope&& other) noexcept
    : previous_(std::move(other.previous_)),
      active_(std::exchange(other.active_, false)) {}

ConnectionScope& ConnectionScope::operator=(
    ConnectionScope&& other
) noexcept {
    if (this == &other) {
        return *this;
    }

    if (active_) {
        scoped_connection = std::move(previous_);
    }

    previous_ = std::move(other.previous_);
    active_ = std::exchange(other.active_, false);
    return *this;
}

void use(Manager& manager) noexcept {
    active_manager = &manager;
}

void clear() noexcept {
    active_manager = nullptr;
    scoped_connection.reset();
}

bool configured() noexcept {
    return active_manager != nullptr;
}

bool using_manager(const Manager& manager) noexcept {
    return active_manager == &manager;
}

Manager& manager() {
    if (active_manager == nullptr) {
        throw std::logic_error(
            "Gungnir database runtime is not configured"
        );
    }

    return *active_manager;
}

std::shared_ptr<Connection> connection(std::string_view name) {
    if (
        scoped_connection &&
        scoped_connection->name() == name
    ) {
        return scoped_connection;
    }

    return manager().connection(name);
}

} // namespace gungnir::database::runtime
