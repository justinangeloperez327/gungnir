#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <gungnir/core/cancellation.hpp>
#include <gungnir/core/types.hpp>
#include <gungnir/database/backend.hpp>
#include <gungnir/database/result.hpp>

namespace gungnir::database {

class Driver {
public:
    virtual ~Driver() = default;

    [[nodiscard]] virtual Backend backend() const noexcept = 0;

    virtual Result execute(
        const String& statement,
        const std::vector<model::AttributeValue>& bindings = {}
    ) = 0;

    virtual void cancel() noexcept {}

    [[nodiscard]] virtual bool supports_transactions() const noexcept {
        return true;
    }

    [[nodiscard]] virtual bool supports_savepoints() const noexcept {
        return false;
    }

    virtual void begin() = 0;
    virtual void commit() = 0;
    virtual void rollback() = 0;

    [[nodiscard]] virtual bool ping() = 0;
};

using DriverFactory = std::function<std::shared_ptr<Driver>()>;

class CallbackDriver final : public Driver {
public:
    using Execute = std::function<Result(
        const String&,
        const std::vector<model::AttributeValue>&
    )>;
    using Action = std::function<void()>;
    using Ping = std::function<bool()>;

    CallbackDriver(
        Backend backend,
        Execute execute,
        Action begin = {},
        Action commit = {},
        Action rollback = {},
        Ping ping = {},
        Action cancel = {}
    );

    [[nodiscard]] Backend backend() const noexcept override;

    Result execute(
        const String& statement,
        const std::vector<model::AttributeValue>& bindings = {}
    ) override;

    void begin() override;
    void commit() override;
    void rollback() override;
    void cancel() noexcept override;
    [[nodiscard]] bool ping() override;

private:
    Backend backend_;
    Execute execute_;
    Action begin_;
    Action commit_;
    Action rollback_;
    Ping ping_;
    Action cancel_;
};

} // namespace gungnir::database
