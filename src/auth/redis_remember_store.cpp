#include <gungnir/auth/redis_remember_store.hpp>

#include <algorithm>
#include <stdexcept>

namespace gungnir::auth {
RedisRememberStore::RedisRememberStore(RedisRememberSettings settings)
    : prefix_(std::move(settings.prefix)), client_(std::move(settings.client)) {
    if (prefix_.empty()) throw std::invalid_argument("Remember-token prefix cannot be empty");
}
std::string RedisRememberStore::key(std::string_view digest) const {
    if (digest.size() != 64 || !std::all_of(digest.begin(), digest.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    })) throw std::invalid_argument("Invalid remember-token digest");
    return prefix_ + std::string{digest};
}
void RedisRememberStore::put(std::string digest, std::string identity, std::chrono::system_clock::time_point expires) {
    auto address = key(digest);
    if (identity.empty()) throw std::invalid_argument("Remember-token identity cannot be empty");
    if (expires <= std::chrono::system_clock::now()) {
        (void)client_.command({"DEL", std::move(address)}); return;
    }
    const auto milliseconds = std::chrono::ceil<std::chrono::milliseconds>(expires.time_since_epoch()).count();
    const auto reply = client_.command({"SET", std::move(address), std::move(identity), "PXAT", std::to_string(milliseconds)});
    if (!reply.ok()) throw std::runtime_error("Remember-token publication failed");
}
std::optional<std::string> RedisRememberStore::consume(std::string_view digest) {
    auto reply = client_.command({"GETDEL", key(digest)});
    if (reply.type == redis::ReplyType::nil) return std::nullopt;
    if (reply.type != redis::ReplyType::string || reply.text.empty()) throw std::runtime_error("Invalid remember-token record");
    return std::move(reply.text);
}
void RedisRememberStore::revoke(std::string_view digest) { (void)client_.command({"DEL", key(digest)}); }
bool RedisRememberStore::ping() { return client_.ping(); }
}
