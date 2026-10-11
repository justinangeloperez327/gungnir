#pragma once

#include <gungnir/http/rate_limit.hpp>
#include <gungnir/redis/client.hpp>

namespace gungnir::http {
struct RedisRateLimitSettings {
    redis::ClientOptions client;
    std::string prefix{"gungnir:http:rate-limit:"};
};

class RedisRateLimitStore final : public RateLimitStore {
public:
    explicit RedisRateLimitStore(RedisRateLimitSettings settings = {});
    [[nodiscard]] RateLimitDecision consume(std::string_view key, std::size_t requests,
        std::chrono::seconds window) override;
    [[nodiscard]] bool ping();
private:
    std::string prefix_;
    redis::Client client_;
};
}
