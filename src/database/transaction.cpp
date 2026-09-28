#include <gungnir/database/transaction.hpp>

#include <exception>
#include <thread>

namespace gungnir::database {

Transaction::Transaction(
    std::shared_ptr<Connection> connection
)
    : connection_(
        std::move(
            connection
        )
      ),
      lock_(
        connection_->mutex_
      ),
      owner_(
        std::this_thread::
            get_id()
      ),
      active_(true) {
    connection_->begin();
}

Transaction::~Transaction() {
    if (
        !active_ ||
        !connection_
    ) {
        return;
    }

    if (
        owner_ !=
        std::this_thread::
            get_id()
    ) {
        std::terminate();
    }

    try {
        connection_->rollback();
    } catch (...) {
    }
}

Connection&
Transaction::connection() {
    ensure_owner();

    return *connection_;
}

const Connection&
Transaction::connection() const {
    ensure_owner();

    return *connection_;
}

void Transaction::commit() {
    ensure_owner();

    if (!active_) {
        return;
    }

    connection_->commit();
    active_ = false;
    lock_.unlock();
}

void Transaction::rollback() {
    ensure_owner();

    if (!active_) {
        return;
    }

    connection_->rollback();
    active_ = false;
    lock_.unlock();
}

bool Transaction::active()
    const noexcept {
    return active_;
}

void Transaction::ensure_owner()
    const {
    if (
        owner_ !=
        std::this_thread::
            get_id()
    ) {
        throw std::logic_error(
            "Database transaction cannot migrate across threads"
        );
    }
}

} // namespace gungnir::database
