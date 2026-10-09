#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace gungnir::scheduler {

struct LockLease {
    std::string key;
    std::string owner;
};

class LockStore {
public:
    virtual ~LockStore() = default;

    [[nodiscard]]
    virtual std::optional<LockLease>
    acquire(
        std::string key,
        std::chrono::milliseconds ttl
    ) = 0;

    [[nodiscard]]
    virtual bool renew(
        const LockLease& lease,
        std::chrono::milliseconds ttl
    ) = 0;

    [[nodiscard]]
    virtual bool release(
        const LockLease& lease
    ) = 0;
};

class MemoryLockStore final :
    public LockStore {
public:
    [[nodiscard]]
    std::optional<LockLease>
    acquire(
        std::string key,
        std::chrono::milliseconds ttl
    ) override {
        validate(
            key,
            ttl
        );

        const auto now =
            Clock::now();

        std::lock_guard lock{
            mutex_
        };

        const auto found =
            locks_.find(key);

        if (
            found != locks_.end() &&
            found->second.expires_at >
                now
        ) {
            return std::nullopt;
        }

        const auto owner =
            std::to_string(
                next_owner_.fetch_add(
                    1,
                    std::memory_order_relaxed
                )
            );

        locks_.insert_or_assign(
            key,
            Entry{
                owner,
                now + ttl
            }
        );

        return LockLease{
            std::move(key),
            owner
        };
    }

    [[nodiscard]]
    bool renew(
        const LockLease& lease,
        std::chrono::milliseconds ttl
    ) override {
        validate(
            lease.key,
            ttl
        );

        std::lock_guard lock{
            mutex_
        };

        const auto found =
            locks_.find(
                lease.key
            );

        if (
            found ==
                locks_.end() ||
            found->second.owner !=
                lease.owner ||
            found->second.expires_at <=
                Clock::now()
        ) {
            return false;
        }

        found->second.expires_at =
            Clock::now() +
            ttl;

        return true;
    }

    [[nodiscard]]
    bool release(
        const LockLease& lease
    ) override {
        std::lock_guard lock{
            mutex_
        };

        const auto found =
            locks_.find(
                lease.key
            );

        if (
            found ==
                locks_.end() ||
            found->second.owner !=
                lease.owner
        ) {
            return false;
        }

        const bool active = found->second.expires_at > Clock::now();
        locks_.erase(found);
        return active;
    }

    [[nodiscard]]
    bool locked(
        std::string_view key
    ) const {
        std::lock_guard lock{
            mutex_
        };

        const auto found =
            locks_.find(
                std::string{key}
            );

        return
            found != locks_.end() &&
            found->second.expires_at >
                Clock::now();
    }

private:
    using Clock =
        std::chrono::steady_clock;

    struct Entry {
        std::string owner;
        Clock::time_point expires_at;
    };

    static void validate(
        const std::string& key,
        std::chrono::milliseconds ttl
    ) {
        if (key.empty()) {
            throw std::invalid_argument(
                "Scheduler lock key must not be empty"
            );
        }

        if (ttl.count() <= 0) {
            throw std::invalid_argument(
                "Scheduler lock TTL must be greater than zero"
            );
        }
    }

    mutable std::mutex mutex_;
    std::unordered_map<
        std::string,
        Entry
    > locks_;
    std::atomic<std::uint64_t>
        next_owner_{1};
};

} // namespace gungnir::scheduler
