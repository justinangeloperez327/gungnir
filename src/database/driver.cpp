#include <gungnir/database/driver.hpp>

#include <stdexcept>
#include <utility>

namespace gungnir::database {

CallbackDriver::CallbackDriver(
    Backend backend,
    Execute execute,
    Action begin,
    Action commit,
    Action rollback,
    Ping ping
)
    : backend_(backend),
      execute_(std::move(execute)),
      begin_(std::move(begin)),
      commit_(std::move(commit)),
      rollback_(std::move(rollback)),
      ping_(std::move(ping)) {
    if (!execute_) {
        throw std::invalid_argument(
            "Gungnir database driver requires an execute callback"
        );
    }
}

Backend CallbackDriver::backend() const noexcept {
    return backend_;
}

Result CallbackDriver::execute(
    const String& statement,
    const std::vector<model::AttributeValue>& bindings
) {
    return execute_(statement, bindings);
}

void CallbackDriver::begin() {
    if (begin_) {
        begin_();
    }
}

void CallbackDriver::commit() {
    if (commit_) {
        commit_();
    }
}

void CallbackDriver::rollback() {
    if (rollback_) {
        rollback_();
    }
}

bool CallbackDriver::ping() {
    return ping_ ? ping_() : true;
}

} // namespace gungnir::database
