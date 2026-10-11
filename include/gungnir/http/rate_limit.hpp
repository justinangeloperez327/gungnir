#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace gungnir::http {

struct RateLimitDecision {
    bool allowed;
    std::size_t remaining;
    std::chrono::seconds retry_after;
};

inline void validate_rate_limit(std::size_t requests, std::chrono::seconds window) {
    // Shared backends use exact integer arithmetic, including Redis Lua numbers.
    constexpr std::uint64_t max_exact_integer = 9007199254740991ULL;
    using FloatingSeconds = std::chrono::duration<long double>;
    const auto remaining = FloatingSeconds{std::chrono::steady_clock::time_point::max().time_since_epoch()}
        - FloatingSeconds{std::chrono::steady_clock::now().time_since_epoch()};
    if (requests == 0 || requests > max_exact_integer || window.count() <= 0 ||
        static_cast<std::uint64_t>(window.count()) > max_exact_integer / 1000 ||
        static_cast<long double>(window.count()) > remaining.count() - 1.0L)
        throw std::invalid_argument("Rate limit values are out of range");
}

class RateLimitStore {
public:
    virtual ~RateLimitStore() = default;
    // A decision and the first-hit window's expiration are one atomic operation.
    [[nodiscard]] virtual RateLimitDecision consume(std::string_view key,
        std::size_t requests, std::chrono::seconds window) = 0;
};

class MemoryRateLimitStore final : public RateLimitStore {
public:
    explicit MemoryRateLimitStore(std::size_t max_clients = 10000) : max_clients_(max_clients) {
        if (max_clients == 0) throw std::invalid_argument("Rate limit client capacity must be positive");
    }
    [[nodiscard]] RateLimitDecision consume(std::string_view key, std::size_t requests,
        std::chrono::seconds window) override {
        validate_rate_limit(requests, window);
        if (key.empty()) throw std::invalid_argument("Rate limit key cannot be empty");
        std::lock_guard guard{mutex_};
        const auto now = std::chrono::steady_clock::now();
        auto found = buckets_.find(std::string{key});
        if (found == buckets_.end() && buckets_.size() >= max_clients_) {
            std::erase_if(buckets_, [&](const auto& item) { return item.second.reset <= now; });
            found = buckets_.find(std::string{key});
        }
        if (found == buckets_.end() && buckets_.size() >= max_clients_) return {false, 0, window};
        auto& bucket = buckets_[std::string{key}];
        if (bucket.reset <= now) bucket = Bucket{0, now + window};
        const bool allowed = bucket.count < requests;
        // Denied requests neither extend the window nor overflow the counter.
        if (allowed) ++bucket.count;
        return {allowed, bucket.count >= requests ? 0 : requests - bucket.count,
            std::chrono::ceil<std::chrono::seconds>(bucket.reset - now)};
    }
private:
    struct Bucket {
        std::size_t count{};
        std::chrono::steady_clock::time_point reset{};
    };
    std::size_t max_clients_;
    std::mutex mutex_;
    std::unordered_map<std::string, Bucket> buckets_;
};
}
