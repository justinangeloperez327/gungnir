#include <gungnir/database/transaction.hpp>

#include <utility>

namespace gungnir::database {

Transaction::Transaction(std::shared_ptr<Connection> connection)
    : connection_(std::move(connection)),
      lock_(connection_->mutex_),
      active_(true) {
    connection_->begin();
}

Transaction::~Transaction() {
    if (active_ && connection_) {
        try {
            connection_->rollback();
        } catch (...) {
        }
    }
}

Transaction::Transaction(Transaction&& other) noexcept
    : connection_(std::move(other.connection_)),
      lock_(std::move(other.lock_)),
      active_(std::exchange(other.active_, false)) {}

Transaction& Transaction::operator=(Transaction&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (active_ && connection_) {
        try {
            connection_->rollback();
        } catch (...) {
        }
    }

    connection_ = std::move(other.connection_);
    lock_ = std::move(other.lock_);
    active_ = std::exchange(other.active_, false);
    return *this;
}

Connection& Transaction::connection() noexcept {
    return *connection_;
}

const Connection& Transaction::connection() const noexcept {
    return *connection_;
}

void Transaction::commit() {
    if (!active_) {
        return;
    }

    connection_->commit();
    active_ = false;
    lock_.unlock();
}

void Transaction::rollback() {
    if (!active_) {
        return;
    }

    connection_->rollback();
    active_ = false;
    lock_.unlock();
}

bool Transaction::active() const noexcept {
    return active_;
}

} // namespace gungnir::database
