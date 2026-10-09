#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <gungnir/core/types.hpp>
#include <gungnir/scheduler/lock.hpp>

namespace gungnir::cache {

// Cache and scheduler leases use the same owner-checked backend contract.
using LockStore = scheduler::LockStore;
using MemoryLockStore = scheduler::MemoryLockStore;

class LockUnavailable : public std::runtime_error {
public:
    LockUnavailable() : std::runtime_error("Cache lock is already held") {}
};
class LockLost : public std::runtime_error {
public:
    LockLost() : std::runtime_error("Cache lock lease expired or was lost") {}
};

// Copies share one lease. The last handle releases it, including during unwind.
class Lock {
public:
    Lock(std::shared_ptr<LockStore> store, std::string key, std::chrono::milliseconds ttl)
        : state_(std::make_shared<State>(std::move(store), std::move(key), ttl)) {}

    [[nodiscard]] bool acquire() const {
        std::lock_guard guard{state_->mutex};
        if (state_->lease) return false;
        state_->lease = state_->store->acquire(state_->key, state_->ttl);
        return state_->lease.has_value();
    }

    [[nodiscard]] bool renew() const {
        std::lock_guard guard{state_->mutex};
        return renew_unlocked(state_->ttl);
    }
    [[nodiscard]] bool renew(Int64 milliseconds) const {
        const auto ttl = duration(milliseconds);
        std::lock_guard guard{state_->mutex};
        return renew_unlocked(ttl);
    }

    [[nodiscard]] bool release() const {
        std::lock_guard guard{state_->mutex};
        if (!state_->lease) return false;
        const bool released = state_->store->release(*state_->lease);
        state_->lease.reset();
        return released;
    }

    [[nodiscard]] static std::chrono::milliseconds duration(Int64 milliseconds) {
        using FloatingMilliseconds = std::chrono::duration<long double, std::milli>;
        const auto remaining = FloatingMilliseconds{std::chrono::steady_clock::time_point::max().time_since_epoch()}
            - FloatingMilliseconds{std::chrono::steady_clock::now().time_since_epoch()};
        if (milliseconds <= 0 || static_cast<long double>(milliseconds) > remaining.count() - 1.0L)
            throw std::invalid_argument("Cache lock lifetime is out of range");
        return std::chrono::milliseconds{milliseconds};
    }

private:
    struct State {
        std::shared_ptr<LockStore> store;
        std::string key;
        std::chrono::milliseconds ttl;
        std::optional<scheduler::LockLease> lease;
        std::mutex mutex;

        State(std::shared_ptr<LockStore> store, std::string key, std::chrono::milliseconds ttl)
            : store(std::move(store)), key(std::move(key)), ttl(duration(ttl.count())) {
            if (!this->store) throw std::logic_error("Cache lock store is not configured");
            if (this->key.empty()) throw std::invalid_argument("Cache lock key cannot be empty");
        }
        ~State() {
            // Destructors cannot report backend errors. Explicit release does.
            try { if (lease) (void)store->release(*lease); } catch (...) {}
        }
    };

    bool renew_unlocked(std::chrono::milliseconds ttl) const {
        if (!state_->lease) return false;
        if (!state_->store->renew(*state_->lease, ttl)) {
            state_->lease.reset();
            return false;
        }
        state_->ttl = ttl;
        return true;
    }
    std::shared_ptr<State> state_;
};
}
