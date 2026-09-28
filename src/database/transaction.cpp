#include <gungnir/database/transaction.hpp>

#include <stdexcept>
#include <utility>

namespace gungnir::database {

Transaction::Transaction(
    std::shared_ptr<Connection> connection
)
    : connection_(
        std::move(connection)
      ) {
    if (!connection_) {
        throw std::invalid_argument(
            "Database transaction requires a connection"
        );
    }

    token_ =
        connection_->begin_scope();

    active_ = true;
}

Transaction::~Transaction() {
    if (!active_) {
        return;
    }

    try {
        rollback();
    } catch (...) {
    }
}

Connection&
Transaction::connection() {
    return *connection_;
}

const Connection&
Transaction::connection()
    const {
    return *connection_;
}

void Transaction::commit() {
    if (!active_) {
        return;
    }

    connection_->
        commit_scope(
            token_
        );

    active_ = false;
}

void Transaction::rollback() {
    if (!active_) {
        return;
    }

    connection_->
        rollback_scope(
            token_
        );

    active_ = false;
}

bool Transaction::active()
    const noexcept {
    return active_;
}

AsyncTransaction::AsyncTransaction(
    std::shared_ptr<Connection> connection,
    AsyncTransactionOptions options
)
    : connection_(
        std::move(connection)
      ),
      options_(
        std::move(options)
      ),
      started_(
        std::chrono::
            steady_clock::now()
      ) {
    if (!connection_) {
        throw std::invalid_argument(
            "Async database transaction requires a connection"
        );
    }

    if (
        options_.timeout.count() <= 0
    ) {
        throw std::invalid_argument(
            "Async database transaction timeout must be greater than zero"
        );
    }

    token_ =
        connection_->begin_scope();

    active_ = true;
}

AsyncTransaction::~AsyncTransaction() {
    if (!active_) {
        return;
    }

    try {
        rollback();
    } catch (...) {
    }
}

Connection&
AsyncTransaction::connection() {
    ensure_not_expired();
    return *connection_;
}

const Connection&
AsyncTransaction::connection()
    const {
    ensure_not_expired();
    return *connection_;
}

void AsyncTransaction::commit() {
    if (!active_) {
        return;
    }

    ensure_not_expired();

    connection_->
        commit_scope(
            token_
        );

    active_ = false;
}

void AsyncTransaction::rollback() {
    if (!active_) {
        return;
    }

    connection_->
        rollback_scope(
            token_
        );

    active_ = false;
}

bool AsyncTransaction::active()
    const noexcept {
    return active_;
}

bool AsyncTransaction::expired()
    const noexcept {
    return
        std::chrono::
            steady_clock::now() -
        started_ >=
        options_.timeout;
}

void AsyncTransaction::ensure_not_expired()
    const {
    if (expired()) {
        throw std::runtime_error(
            "Async database transaction exceeded its timeout"
        );
    }
}

} // namespace gungnir::database
