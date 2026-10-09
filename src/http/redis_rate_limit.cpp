#include <gungnir/http/redis_rate_limit.hpp>

#include <algorithm>
#include <utility>

namespace gungnir::http {
namespace {
// One key/one script: Redis chooses the window start and TTL for every client.
// A lost response is reported as an error; transport failures are not replayed.
constexpr const char* consume_script = R"lua(
local limit = tonumber(ARGV[1])
local value = redis.call('GET', KEYS[1])
if not value then
    redis.call('SET', KEYS[1], '1', 'PX', ARGV[2])
    return {1, limit - 1, tonumber(ARGV[2])}
end
local count = tonumber(value)
local ttl = redis.call('PTTL', KEYS[1])
if not count or count < 0 or count ~= math.floor(count) or ttl < 0 then
    return redis.error_reply('Invalid rate limit record')
end
if count >= limit then return {0, 0, ttl} end
redis.call('INCR', KEYS[1])
return {1, limit - count - 1, ttl}
)lua";
}

RedisRateLimitStore::RedisRateLimitStore(RedisRateLimitSettings settings)
    : prefix_(std::move(settings.prefix)), client_(std::move(settings.client)) {
    if (prefix_.empty()) throw std::invalid_argument("Redis rate limit prefix cannot be empty");
}

RateLimitDecision RedisRateLimitStore::consume(std::string_view key, std::size_t requests,
    std::chrono::seconds window) {
    validate_rate_limit(requests, window);
    if (key.empty()) throw std::invalid_argument("Rate limit key cannot be empty");
    const auto reply = client_.command({"EVAL", consume_script, "1", prefix_ + std::string{key},
        std::to_string(requests), std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(window).count())});
    if (reply.type != redis::ReplyType::array || reply.elements.size() != 3)
        throw std::runtime_error("Redis rate limit returned an unexpected reply");
    for (const auto& item : reply.elements)
        if (item.type != redis::ReplyType::integer) throw std::runtime_error("Redis rate limit returned an invalid decision");
    const auto allowed = reply.elements[0].integer;
    const auto remaining = reply.elements[1].integer;
    const auto milliseconds = reply.elements[2].integer;
    if ((allowed != 0 && allowed != 1) || remaining < 0 || static_cast<std::uint64_t>(remaining) > requests || milliseconds < 0)
        throw std::runtime_error("Redis rate limit returned an invalid decision");
    return {allowed == 1, static_cast<std::size_t>(remaining),
        std::max(std::chrono::seconds{1}, std::chrono::ceil<std::chrono::seconds>(std::chrono::milliseconds{milliseconds}))};
}
bool RedisRateLimitStore::ping() { return client_.ping(); }
}
