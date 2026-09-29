#include <gungnir/database/runtime.hpp>

#include <atomic>
#include <stdexcept>
#include <utility>

namespace gungnir::database::runtime {

namespace {

std::atomic<Manager*> active_manager{
    nullptr
};

thread_local ConnectionHandle
    current_connection;

} // namespace

namespace detail {

ConnectionScope::ConnectionScope(
    ConnectionHandle connection
) noexcept
    : previous_(
        std::move(
            current_connection
        )
      ),
      active_(true) {
    current_connection =
        std::move(connection);
}

ConnectionScope::ConnectionScope(
    ConnectionScope&& other
) noexcept
    : previous_(
        std::move(
            other.previous_
        )
      ),
      active_(
        std::exchange(
            other.active_,
            false
        )
      ) {}

ConnectionScope&
ConnectionScope::operator=(
    ConnectionScope&& other
) noexcept {
    if (this == &other) {
        return *this;
    }

    reset();

    previous_ =
        std::move(
            other.previous_
        );

    active_ =
        std::exchange(
            other.active_,
            false
        );

    return *this;
}

ConnectionScope::~ConnectionScope() {
    reset();
}

void ConnectionScope::reset()
    noexcept {
    if (!active_) {
        return;
    }

    current_connection =
        std::move(previous_);

    active_ = false;
}

} // namespace detail

void use(
    Manager& manager
) noexcept {
    active_manager.store(
        &manager,
        std::memory_order_release
    );
}

void clear()
    noexcept {
    active_manager.store(
        nullptr,
        std::memory_order_release
    );

    current_connection.reset();
}

bool configured()
    noexcept {
    return
        active_manager.load(
            std::memory_order_acquire
        ) != nullptr;
}

bool using_manager(
    const Manager& manager
) noexcept {
    return
        active_manager.load(
            std::memory_order_acquire
        ) ==
        &manager;
}

Manager& manager() {
    auto* current =
        active_manager.load(
            std::memory_order_acquire
        );

    if (current == nullptr) {
        throw std::logic_error(
            "Gungnir database runtime is not configured"
        );
    }

    return *current;
}

ConnectionHandle current()
    noexcept {
    return current_connection;
}

detail::ConnectionScope activate(
    ConnectionHandle connection
) noexcept {
    return
        detail::ConnectionScope{
            std::move(connection)
        };
}

void clear_current()
    noexcept {
    current_connection.reset();
}

ConnectionHandle connection(
    std::string_view name
) {
    if (
        current_connection &&
        current_connection->name() ==
            name
    ) {
        return current_connection;
    }

    return
        manager().connection(
            name
        );
}

ConnectionHandle read_connection(
    std::string_view name
) {
    if (
        current_connection &&
        current_connection->name() ==
            name
    ) {
        return current_connection;
    }

    return
        manager().read_connection(
            name
        );
}

ConnectionHandle write_connection(
    std::string_view name
) {
    if (
        current_connection &&
        current_connection->name() ==
            name
    ) {
        return current_connection;
    }

    return
        manager().write_connection(
            name
        );
}

} // namespace gungnir::database::runtime
