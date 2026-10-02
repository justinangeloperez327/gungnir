#include <gungnir/database/connection.hpp>

#include <gungnir/database/error.hpp>
#include <gungnir/observability/metrics.hpp>
#include <gungnir/observability/trace.hpp>

#include <cctype>
#include <stdexcept>
#include <utility>

namespace gungnir::database {

namespace {

[[nodiscard]]
std::string operation_name(
    const String& statement
) {
    std::size_t cursor = 0;

    while (
        cursor < statement.size() &&
        std::isspace(
            static_cast<unsigned char>(
                statement[cursor]
            )
        )
    ) {
        ++cursor;
    }

    const auto start = cursor;

    while (
        cursor < statement.size() &&
        !std::isspace(
            static_cast<unsigned char>(
                statement[cursor]
            )
        )
    ) {
        ++cursor;
    }

    auto operation =
        statement.substr(
            start,
            cursor - start
        );

    for (auto& character : operation) {
        character =
            static_cast<char>(
                std::toupper(
                    static_cast<unsigned char>(
                        character
                    )
                )
            );
    }

    return operation;
}

[[nodiscard]]
observability::Span database_span(
    const Connection& connection,
    const String& statement
) {
    return
        observability::
            global_tracer()
            ->start_span(
                "database.query",
                {
                    {
                        "db.system",
                        std::string{
                            name(
                                connection.backend()
                            )
                        }
                    },
                    {
                        "db.connection.name",
                        connection.name()
                    },
                    {
                        "db.operation",
                        operation_name(
                            statement
                        )
                    }
                }
            );
}

void record_database_metrics(
    const Connection& connection,
    const String& statement,
    std::chrono::steady_clock::time_point started,
    std::string outcome
) {
    auto attributes =
        observability::Attributes{
            {
                "db.system",
                std::string{
                    name(
                        connection.backend()
                    )
                }
            },
            {
                "db.connection.name",
                connection.name()
            },
            {
                "db.operation",
                operation_name(
                    statement
                )
            },
            {
                "outcome",
                std::move(outcome)
            }
        };

    const auto elapsed =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() -
            started
        ).count();

    auto meter =
        observability::global_meter();

    meter->histogram(
        "db.client.operation.duration"
    ).record(
        elapsed,
        attributes
    );

    meter->counter(
        "db.client.operation.count"
    ).add(
        1.0,
        std::move(attributes)
    );
}

} // namespace

Connection::Connection(
    String name,
    std::shared_ptr<Driver> driver
)
    : name_(std::move(name)),
      driver_(std::move(driver)) {
    if (!driver_) {
        throw std::invalid_argument(
            "Gungnir database connection requires a driver"
        );
    }
}

const String& Connection::name() const noexcept {
    return name_;
}

Backend Connection::backend() const noexcept {
    return driver_->backend();
}

Result Connection::execute(
    const String& statement,
    const std::vector<model::AttributeValue>& bindings
) {
    std::lock_guard lock{mutex_};

    const auto metrics_started =
        std::chrono::steady_clock::now();

    auto span =
        database_span(
            *this,
            statement
        );

    auto scope =
        span.valid()
            ? span.scope()
            : observability::Scope{};

    try {
        auto result =
            driver_->execute(
                statement,
                bindings
            );

        span.status(
            observability::
                SpanStatus::ok
        );

        span.end();

        record_database_metrics(
            *this,
            statement,
            metrics_started,
            "ok"
        );

        return result;
    } catch (const Error& error) {
        span.error(
            error.what()
        );

        span.end();

        record_database_metrics(
            *this,
            statement,
            metrics_started,
            "error"
        );

        throw;
    } catch (const std::exception& error) {
        span.error(
            error.what()
        );

        span.end();

        record_database_metrics(
            *this,
            statement,
            metrics_started,
            "error"
        );

        throw Error{
            "Database execution failed: " +
                String{error.what()},
            backend(),
            name_,
            statement
        };
    }
}

Result Connection::execute(
    const String& statement,
    const std::vector<model::AttributeValue>& bindings,
    const CancellationToken& cancellation
) {
    std::lock_guard lock{mutex_};

    const auto metrics_started =
        std::chrono::steady_clock::now();

    auto span =
        database_span(
            *this,
            statement
        );

    auto scope =
        span.valid()
            ? span.scope()
            : observability::Scope{};

    cancellation.throw_if_cancelled();

    auto registration =
        cancellation.on_cancel(
            [driver = driver_]() noexcept {
                driver->cancel();
            }
        );

    cancellation.throw_if_cancelled();

    try {
        auto result =
            driver_->execute(
                statement,
                bindings
            );

        cancellation.throw_if_cancelled();

        span.status(
            observability::
                SpanStatus::ok
        );

        span.end();

        record_database_metrics(
            *this,
            statement,
            metrics_started,
            "ok"
        );

        return result;
    } catch (
        const OperationCancelled&
    ) {
        span.error(
            "Database operation cancelled"
        );

        span.end();

        record_database_metrics(
            *this,
            statement,
            metrics_started,
            "cancelled"
        );

        throw;
    } catch (const Error& error) {
        if (cancellation.cancelled()) {
            span.error(
                "Database operation cancelled"
            );

            span.end();

            record_database_metrics(
                *this,
                statement,
                metrics_started,
                "cancelled"
            );

            throw OperationCancelled{};
        }

        span.error(
            error.what()
        );

        span.end();

        record_database_metrics(
            *this,
            statement,
            metrics_started,
            "error"
        );

        throw;
    } catch (
        const std::exception& error
    ) {
        if (cancellation.cancelled()) {
            span.error(
                "Database operation cancelled"
            );

            span.end();

            record_database_metrics(
                *this,
                statement,
                metrics_started,
                "cancelled"
            );

            throw OperationCancelled{};
        }

        span.error(
            error.what()
        );

        span.end();

        record_database_metrics(
            *this,
            statement,
            metrics_started,
            "error"
        );

        throw Error{
            "Database execution failed: " +
                String{error.what()},
            backend(),
            name_,
            statement
        };
    }
}

Result Connection::execute(
    const Query& query
) {
    return execute(
        query.statement,
        query.bindings
    );
}

Result Connection::execute(
    const Query& query,
    const CancellationToken& cancellation
) {
    return execute(
        query.statement,
        query.bindings,
        cancellation
    );
}

bool Connection::supports_transactions() const noexcept {
    return driver_->supports_transactions();
}

bool Connection::supports_savepoints() const noexcept {
    return driver_->supports_savepoints();
}

bool Connection::in_transaction() const {
    std::lock_guard lock{mutex_};
    return transaction_depth_ != 0;
}

void Connection::after_commit(
    std::function<void()> callback
) {
    if (!callback) {
        throw std::invalid_argument(
            "Commit callback cannot be empty"
        );
    }

    {
        std::lock_guard lock{mutex_};

        if (transaction_depth_ != 0) {
            commit_callbacks_.back().push_back(
                std::move(callback)
            );
            return;
        }
    }

    callback();
}

void Connection::begin() {
    std::lock_guard lock{mutex_};

    if (transaction_depth_ != 0) {
        throw std::logic_error(
            "Use transaction scopes for nested transactions"
        );
    }

    static_cast<void>(begin_scope());
}

void Connection::commit() {
    std::lock_guard lock{mutex_};

    if (transaction_depth_ == 0) {
        return;
    }

    if (transaction_depth_ != 1) {
        throw std::logic_error(
            "Cannot commit a root database transaction while a nested scope is active"
        );
    }

    commit_scope(
        TransactionToken{
            .root = true,
            .savepoint = {},
            .depth = 1,
            .scope_id =
                transaction_scope_ids_.back()
        }
    );
}

void Connection::rollback() {
    std::lock_guard lock{mutex_};

    if (transaction_depth_ == 0) {
        return;
    }

    if (transaction_depth_ != 1) {
        throw std::logic_error(
            "Cannot roll back a root database transaction while a nested scope is active"
        );
    }

    rollback_scope(
        TransactionToken{
            .root = true,
            .savepoint = {},
            .depth = 1,
            .scope_id =
                transaction_scope_ids_.back()
        }
    );
}

TransactionToken
Connection::begin_scope() {
    std::lock_guard lock{mutex_};

    if (transaction_depth_ == 0) {
        driver_->begin();
        transaction_depth_ = 1;
        commit_callbacks_.emplace_back();

        const auto scope_id =
            ++transaction_scope_sequence_;
        transaction_scope_ids_.push_back(
            scope_id
        );

        return {
            .root = true,
            .savepoint = {},
            .depth = 1,
            .scope_id = scope_id
        };
    }

    if (!supports_savepoints()) {
        throw std::logic_error(
            "Nested database transactions require savepoint support"
        );
    }

    auto savepoint =
        next_savepoint_name();

    switch (backend()) {
    case Backend::mssql:
        driver_->execute(
            "SAVE TRANSACTION " +
            savepoint
        );
        break;

    case Backend::sqlite:
    case Backend::postgresql:
    case Backend::mysql:
        driver_->execute(
            "SAVEPOINT " +
            savepoint
        );
        break;

    case Backend::mongodb:
        throw std::logic_error(
            "MongoDB nested transactions use session semantics rather than SQL savepoints"
        );
    }

    ++transaction_depth_;
    commit_callbacks_.emplace_back();

    const auto scope_id =
        ++transaction_scope_sequence_;
    transaction_scope_ids_.push_back(
        scope_id
    );

    return {
        .root = false,
        .savepoint = std::move(savepoint),
        .depth = transaction_depth_,
        .scope_id = scope_id
    };
}

void Connection::validate_scope_token(
    const TransactionToken& token
) const {
    if (
        transaction_depth_ == 0 ||
        transaction_scope_ids_.empty()
    ) {
        throw std::logic_error(
            "Database transaction scope is no longer active"
        );
    }

    if (
        token.depth != transaction_depth_ ||
        token.scope_id == 0 ||
        token.scope_id !=
            transaction_scope_ids_.back() ||
        token.root !=
            (transaction_depth_ == 1)
    ) {
        throw std::logic_error(
            "Database transaction scopes must be completed in LIFO order"
        );
    }

    if (
        !token.root &&
        token.savepoint.empty()
    ) {
        throw std::logic_error(
            "Nested database transaction scope is missing its savepoint"
        );
    }
}

void Connection::commit_scope(
    const TransactionToken& token
) {
    std::unique_lock lock{mutex_};

    validate_scope_token(token);

    if (token.root) {
        driver_->commit();
        transaction_depth_ = 0;
        transaction_scope_ids_.clear();

        std::vector<
            std::function<void()>
        > callbacks;

        for (auto& level : commit_callbacks_) {
            for (auto& callback : level) {
                callbacks.push_back(
                    std::move(callback)
                );
            }
        }

        commit_callbacks_.clear();
        lock.unlock();

        std::exception_ptr failure;

        for (auto& callback : callbacks) {
            try {
                callback();
            } catch (...) {
                if (!failure) {
                    failure =
                        std::current_exception();
                }
            }
        }

        if (failure) {
            std::rethrow_exception(
                failure
            );
        }

        return;
    }

    if (
        backend() ==
            Backend::postgresql ||
        backend() ==
            Backend::mysql ||
        backend() ==
            Backend::sqlite
    ) {
        driver_->execute(
            "RELEASE SAVEPOINT " +
            token.savepoint
        );
    }

    auto callbacks =
        std::move(
            commit_callbacks_.back()
        );
    commit_callbacks_.pop_back();

    for (auto& callback : callbacks) {
        commit_callbacks_.back()
            .push_back(
                std::move(callback)
            );
    }

    transaction_scope_ids_.pop_back();
    --transaction_depth_;
}

void Connection::rollback_scope(
    const TransactionToken& token
) {
    std::lock_guard lock{mutex_};

    validate_scope_token(token);

    if (token.root) {
        driver_->rollback();
        transaction_depth_ = 0;
        transaction_scope_ids_.clear();
        commit_callbacks_.clear();
        return;
    }

    switch (backend()) {
    case Backend::mssql:
        driver_->execute(
            "ROLLBACK TRANSACTION " +
            token.savepoint
        );
        break;

    case Backend::sqlite:
    case Backend::postgresql:
    case Backend::mysql:
        driver_->execute(
            "ROLLBACK TO SAVEPOINT " +
            token.savepoint
        );
        break;

    case Backend::mongodb:
        break;
    }

    commit_callbacks_.pop_back();
    transaction_scope_ids_.pop_back();
    --transaction_depth_;
}

String Connection::next_savepoint_name() {
    ++savepoint_sequence_;

    return
        "gungnir_sp_" +
        std::to_string(
            savepoint_sequence_
        );
}

bool Connection::healthy() {
    std::lock_guard lock{mutex_};
    return driver_->ping();
}

} // namespace gungnir::database
