#include <gungnir/database/runtime.hpp>

#include <atomic>
#include <stdexcept>
#include <utility>

namespace gungnir::database::runtime {

namespace {

thread_local std::atomic<Manager*> active_manager{
    nullptr
};



} // namespace

namespace detail {

ConnectionScope::ConnectionScope(
    ConnectionHandle connection
) noexcept
    : owner_(gungnir::detail::current_execution_context()),
      previous_(
        std::move(
            gungnir::detail::active_context->database
        )
      ),
      active_(true) {
    gungnir::detail::active_context->database =
        std::move(connection);
}

ConnectionScope::ConnectionScope(
    ConnectionScope&& other
) noexcept
    : owner_(std::move(other.owner_)),
      previous_(
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

    owner_ = std::move(other.owner_);
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

    owner_->database =
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

    gungnir::detail::active_context->database.reset();
}

bool configured()
    noexcept {
    if (gungnir::detail::application_context()->manager) return true;
    return
        active_manager.load(
            std::memory_order_acquire
        ) != nullptr;
}

bool using_manager(
    const Manager& manager
) noexcept {
    if (auto context = gungnir::detail::application_context(); context->manager) return context->manager.get() == &manager;
    return
        active_manager.load(
            std::memory_order_acquire
        ) ==
        &manager;
}

Manager& manager() {
    if (auto context = gungnir::detail::application_context(); context->manager) return *context->manager;
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
    return gungnir::detail::active_context->database;
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
    gungnir::detail::active_context->database.reset();
}

ConnectionHandle connection(
    std::string_view name
) {
    if (
        gungnir::detail::active_context->database &&
        gungnir::detail::active_context->database->name() ==
            name
    ) {
        return gungnir::detail::active_context->database;
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
        gungnir::detail::active_context->database &&
        gungnir::detail::active_context->database->name() ==
            name
    ) {
        return gungnir::detail::active_context->database;
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
        gungnir::detail::active_context->database &&
        gungnir::detail::active_context->database->name() ==
            name
    ) {
        return gungnir::detail::active_context->database;
    }

    return
        manager().write_connection(
            name
        );
}

} // namespace gungnir::database::runtime

