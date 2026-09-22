#include <gungnir/database/connection.hpp>

#include <gungnir/database/error.hpp>

#include <stdexcept>
#include <utility>

namespace gungnir::database {

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
    try {
        return driver_->execute(statement, bindings);
    } catch (const Error&) {
        throw;
    } catch (const std::exception& error) {
        throw Error{
            "Database execution failed: " + String{error.what()},
            backend(),
            name_,
            statement
        };
    }
}

Result Connection::execute(const Query& query) {
    return execute(query.statement, query.bindings);
}

void Connection::begin() {
    std::lock_guard lock{mutex_};

    if (!driver_->supports_transactions()) {
        throw std::logic_error(
            "Database driver does not support transactions"
        );
    }

    driver_->begin();
}

void Connection::commit() {
    std::lock_guard lock{mutex_};
    driver_->commit();
}

void Connection::rollback() {
    std::lock_guard lock{mutex_};
    driver_->rollback();
}

bool Connection::healthy() {
    std::lock_guard lock{mutex_};
    return driver_->ping();
}

} // namespace gungnir::database
