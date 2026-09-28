#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string_view>

#include <gungnir/cache/redis_store.hpp>
#include <gungnir/session/store.hpp>

namespace gungnir::session {

struct RedisSessionSettings {
    RedisSessionSettings() {
        redis.prefix =
            "gungnir:session:";
    }

    cache::RedisSettings redis;
    std::chrono::seconds lifetime{
        7200
    };
};

class RedisStore final :
    public Store {
public:
    explicit RedisStore(
        RedisSessionSettings settings = {}
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
    std::optional<Session> load(
        std::string_view id
    ) override;

    void save(
        const Session& session
    ) override;

    void erase(
        std::string_view id
    ) override;

    [[nodiscard]]
    bool ping();

    void flush();

    [[nodiscard]]
    const RedisSessionSettings&
    settings() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::session
