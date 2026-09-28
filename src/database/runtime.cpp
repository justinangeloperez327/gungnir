#include <gungnir/database/runtime.hpp>

#include <atomic>
#include <stdexcept>
#include <utility>

namespace gungnir::database::runtime {

namespace {

std::atomic<Manager*> active_manager{
    nullptr
};

thread_local std::shared_ptr<
    Connection
> scoped_connection;

} // namespace

namespace detail {

ConnectionScope::ConnectionScope(
    std::shared_ptr<Connection> connection
)
    : previous_(
        std::move(
            scoped_connection
        )
      ) {
    if (!connection) {
        throw std::invalid_argument(
            "Database connection scope requires a connection"
        );
    }

    scoped_connection =
        std::move(
            connection
        );
}

ConnectionScope::~ConnectionScope() {
    scoped_connection =
        std::move(
            previous_
        );
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

void clear() noexcept {
    active_manager.store(
        nullptr,
        std::memory_order_release
    );

    scoped_connection.reset();
}

bool configured() noexcept {
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

std::shared_ptr<Connection>
connection(
    std::string_view name
) {
    if (
        scoped_connection &&
        scoped_connection->name() ==
            name
    ) {
        return scoped_connection;
    }

    return
        manager().connection(
            name
        );
}

} // namespace gungnir::database::runtime
