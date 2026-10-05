#pragma once

#include <gungnir/auth/login.hpp>
#include <gungnir/redis/client.hpp>

namespace gungnir::auth {
struct RedisRememberSettings {
    redis::ClientOptions client;
    std::string prefix{"gungnir:auth:remember:"};
};

// Redis 6.2+: tokens are addressed by their digest, expire on the server, and
// are consumed with one GETDEL. No raw browser credential is stored here.
class RedisRememberStore final : public RememberStore {
public:
    explicit RedisRememberStore(RedisRememberSettings settings = {});
    void put(std::string digest, std::string identity, std::chrono::system_clock::time_point expires) override;
    std::optional<std::string> consume(std::string_view digest) override;
    void revoke(std::string_view digest) override;
    [[nodiscard]] bool ping();
private:
    [[nodiscard]] std::string key(std::string_view digest) const;
    std::string prefix_;
    redis::Client client_;
};
}
