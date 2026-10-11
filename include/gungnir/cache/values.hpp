#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include <gungnir/cache/repository.hpp>
#include <gungnir/core/types.hpp>
#include <gungnir/http/json.hpp>

namespace gungnir::cache {

// JSON values use the existing repository and retain its adapter owner.
class Values {
public:
    explicit Values(std::shared_ptr<Repository> repository)
        : repository_(std::move(repository)) {
        if (!repository_) throw std::invalid_argument("Cache repository is not configured");
    }

    [[nodiscard]] std::optional<http::Json> get(std::string_view key) const {
        auto value = repository_->get(key);
        return value ? std::optional{http::Json::parse(*value)} : std::nullopt;
    }

    void put(String key, const http::Json& value, std::optional<Int64> seconds = std::nullopt) const {
        const auto ttl = expiration(seconds);
        repository_->put(std::move(key), value.dump(), ttl);
    }

    [[nodiscard]] bool has(std::string_view key) const { return repository_->has(key); }
    bool forget(std::string_view key) const { return repository_->forget(key); }
    void flush() const { repository_->flush(); }

    [[nodiscard]] Lock lock(String key, Int64 milliseconds) const {
        return repository_->lock(std::move(key), Lock::duration(milliseconds));
    }

    template<class Factory>
    [[nodiscard]] http::Json remember(String key, Int64 seconds, Factory&& factory) const {
        const auto ttl = expiration(seconds);
        return http::Json::parse(repository_->remember(std::move(key), ttl, [&factory] {
            return http::make_json(std::invoke(std::forward<Factory>(factory))).dump();
        }));
    }

    template<class Factory>
    [[nodiscard]] http::Json rememberLocked(String key, Int64 seconds, Int64 lease_milliseconds, Factory&& factory) const {
        const auto ttl = expiration(seconds);
        const auto lease_ttl = Lock::duration(lease_milliseconds);
        return http::Json::parse(repository_->remember_locked(std::move(key), ttl, lease_ttl, [&factory] {
            return http::make_json(std::invoke(std::forward<Factory>(factory))).dump();
        }));
    }

private:
    static std::optional<Duration> expiration(std::optional<Int64> seconds) {
        if (!seconds) return std::nullopt;
        using FloatingSeconds = std::chrono::duration<long double>;
        const auto remaining = FloatingSeconds{std::chrono::steady_clock::time_point::max().time_since_epoch()}
            - FloatingSeconds{std::chrono::steady_clock::now().time_since_epoch()};
        if (*seconds < 0 || static_cast<long double>(*seconds) > remaining.count() - 1.0L)
            throw std::invalid_argument("Cache lifetime is out of range");
        return Duration{*seconds};
    }

    std::shared_ptr<Repository> repository_;
};

} // namespace gungnir::cache
