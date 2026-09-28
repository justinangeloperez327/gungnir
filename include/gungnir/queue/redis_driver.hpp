#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <gungnir/queue/driver.hpp>

namespace gungnir::queue {

struct RedisSettings {
    std::string host{"127.0.0.1"};
    std::uint16_t port{6379};
    std::optional<std::string> username;
    std::optional<std::string> password;
    int database{0};
    std::string prefix{"gungnir:queue:"};
    std::string queue{"default"};
    std::chrono::milliseconds visibility_timeout{
        60000
    };
    std::chrono::milliseconds connect_timeout{
        2000
    };
    std::chrono::milliseconds command_timeout{
        2000
    };
};

class RedisDriver final :
    public Driver {
public:
    explicit RedisDriver(
        RedisSettings settings = {}
    );

    ~RedisDriver() override;

    RedisDriver(
        const RedisDriver&
    ) = delete;

    RedisDriver& operator=(
        const RedisDriver&
    ) = delete;

    RedisDriver(
        RedisDriver&&
    ) noexcept;

    RedisDriver& operator=(
        RedisDriver&&
    ) noexcept;

    void push(
        Envelope job
    ) override;

    void push_later(
        Envelope job,
        std::chrono::milliseconds delay
    ) override;

    [[nodiscard]]
    std::optional<Envelope>
    pop() override;

    void acknowledge(
        const Envelope& job
    ) override;

    void release(
        Envelope job
    ) override;

    void release_after(
        Envelope job,
        std::chrono::milliseconds delay
    ) override;

    [[nodiscard]]
    bool renew(
        const Envelope& job
    ) override;

    void fail(
        const Envelope& job
    ) override;

    [[nodiscard]]
    bool ping();

    [[nodiscard]]
    std::size_t pending();

    [[nodiscard]]
    std::size_t reserved();

    [[nodiscard]]
    std::size_t delayed();

    [[nodiscard]]
    std::size_t failed();

    void flush();

    [[nodiscard]]
    const RedisSettings& settings()
        const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::queue
