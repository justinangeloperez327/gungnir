#pragma once
#include <gungnir/database/driver.hpp>
#include <gungnir/database/settings.hpp>

namespace gungnir::database {
class SQLiteDriver final : public Driver {
public:
    explicit SQLiteDriver(const Settings& settings);
    ~SQLiteDriver() override;
    SQLiteDriver(const SQLiteDriver&) = delete;
    SQLiteDriver& operator=(const SQLiteDriver&) = delete;
    [[nodiscard]] Backend backend() const noexcept override { return Backend::sqlite; }
    Result execute(const String& statement, const std::vector<model::AttributeValue>& bindings = {}) override;
    void begin() override;
    void commit() override;
    void rollback() override;
    void cancel() noexcept override;
    [[nodiscard]] bool ping() override;
    [[nodiscard]] bool supports_savepoints() const noexcept override { return true; }
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
}
