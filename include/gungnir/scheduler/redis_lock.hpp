#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <gungnir/scheduler/lock.hpp>

namespace gungnir::scheduler {

struct RedisLockSettings {
    std::string host{"127.0.0.1"};
    std::uint16_t port{6379};
    std::optional<std::string> username;
    std::optional<std::string> password;
    int database{0};
    std::string prefix{"gungnir:scheduler:lock:"};
    std::chrono::milliseconds connect_timeout{2000};
    std::chrono::milliseconds command_timeout{2000};
};

class RedisLockStore final :
    public LockStore {
public:
    explicit RedisLockStore(
        RedisLockSettings settings = {}
    );

    ~RedisLockStore() override;

    RedisLockStore(
        const RedisLockStore&
    ) = delete;

    RedisLockStore& operator=(
        const RedisLockStore&
    ) = delete;

    RedisLockStore(
        RedisLockStore&&
    ) noexcept;

    RedisLockStore& operator=(
        RedisLockStore&&
    ) noexcept;

    [[nodiscard]]
    std::optional<LockLease>
    acquire(
        std::string key,
        std::chrono::milliseconds ttl
    ) override;

    [[nodiscard]]
    bool renew(
        const LockLease& lease,
        std::chrono::milliseconds ttl
    ) override;

    [[nodiscard]]
    bool release(
        const LockLease& lease
    ) override;

    [[nodiscard]]
    bool ping();

    void flush();

    [[nodiscard]]
    const RedisLockSettings& settings()
        const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::scheduler
