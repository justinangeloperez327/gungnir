#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <gungnir/cache/store.hpp>

namespace gungnir::cache {

struct RedisSettings {
    std::string host{"127.0.0.1"};
    std::uint16_t port{6379};
    std::optional<std::string> username;
    std::optional<std::string> password;
    int database{0};
    std::string prefix;
    std::chrono::milliseconds connect_timeout{2000};
    std::chrono::milliseconds command_timeout{2000};
};

class RedisStore final :
    public Store {
public:
    explicit RedisStore(
        RedisSettings settings = {}
    );

    ~RedisStore() override;

    RedisStore(
        const RedisStore&
    ) = delete;

    RedisStore& operator=(
        const RedisStore&
    ) = delete;

    RedisStore(
        RedisStore&&
    ) noexcept;

    RedisStore& operator=(
        RedisStore&&
    ) noexcept;

    [[nodiscard]]
    std::optional<std::string> get(
        std::string_view key
    ) override;

    void put(
        std::string key,
        std::string value,
        std::optional<Duration> ttl =
            std::nullopt
    ) override;

    bool forget(
        std::string_view key
    ) override;

    void flush() override;

    [[nodiscard]]
    bool ping();

    [[nodiscard]]
    const RedisSettings& settings()
        const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::cache
