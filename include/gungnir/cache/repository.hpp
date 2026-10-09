#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <gungnir/cache/store.hpp>
#include <gungnir/cache/lock.hpp>
#include <gungnir/observability/metrics.hpp>

namespace gungnir::cache {

class Repository {
public:
    explicit Repository(
        Store& store
    )
        : store_(&store) {}

    Repository(Store& store, std::shared_ptr<LockStore> locks)
        : store_(&store), locks_(std::move(locks)) {}

    [[nodiscard]] Lock lock(std::string key, std::chrono::milliseconds ttl) const {
        return Lock{locks_, std::move(key), ttl};
    }

    [[nodiscard]]
    std::optional<std::string>
    get(
        std::string_view key
    ) {
        auto value =
            store_->get(key);

        observability::
            global_meter()
            ->counter(
                "cache.request.count"
            )
            .add(
                1.0,
                {
                    {
                        "cache.result",
                        value
                            ? "hit"
                            : "miss"
                    }
                }
            );

        return value;
    }

    void put(
        std::string key,
        std::string value,
        std::optional<Duration> ttl =
            std::nullopt
    ) {
        store_->put(
            std::move(key),
            std::move(value),
            ttl
        );
    }

    [[nodiscard]]
    bool has(
        std::string_view key
    ) {
        return
            get(key).has_value();
    }

    bool forget(
        std::string_view key
    ) {
        return
            store_->forget(key);
    }

    void flush() {
        store_->flush();
    }

    template <typename Factory>
    [[nodiscard]]
    std::string remember(
        std::string key,
        std::optional<Duration> ttl,
        Factory&& factory
    ) {
        if (
            auto cached =
                get(key)
        ) {
            return
                std::move(
                    *cached
                );
        }

        auto value =
            std::invoke(
                std::forward<Factory>(
                    factory
                )
            );

        store_->put(
            std::move(key),
            value,
            ttl
        );

        return value;
    }

    template <typename Factory>
    [[nodiscard]] std::string remember_locked(std::string key, std::optional<Duration> ttl,
        std::chrono::milliseconds lease_ttl, Factory&& factory) {
        // Validate configuration even for warm entries.
        auto lease = lock("remember:" + key, lease_ttl);
        if (auto cached = get(key)) return std::move(*cached);
        if (!lease.acquire()) throw LockUnavailable{};
        // A contender may have filled the value between our read and acquisition.
        if (auto cached = get(key)) return std::move(*cached);
        auto value = std::invoke(std::forward<Factory>(factory));
        if (!lease.renew()) throw LockLost{};
        store_->put(std::move(key), value, ttl);
        if (!lease.release()) throw LockLost{};
        return value;
    }

private:
    Store* store_;
    std::shared_ptr<LockStore> locks_;
};

} // namespace gungnir::cache
