#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include <gungnir/core/types.hpp>
#include <gungnir/database/driver.hpp>
#include <gungnir/database/result.hpp>

namespace gungnir::database {

class Transaction;

class Connection {
public:
    Connection(String name, std::shared_ptr<Driver> driver);

    [[nodiscard]] const String& name() const noexcept;
    [[nodiscard]] Backend backend() const noexcept;

    Result execute(
        const String& statement,
        const std::vector<model::AttributeValue>& bindings = {}
    );

    void begin();
    void commit();
    void rollback();

    [[nodiscard]] bool healthy();

private:
    friend class Transaction;

    String name_;
    std::shared_ptr<Driver> driver_;
    std::recursive_mutex mutex_;
};

} // namespace gungnir::database
