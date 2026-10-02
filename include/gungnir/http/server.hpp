#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <gungnir/core/cancellation.hpp>
#include <gungnir/http/runtime.hpp>

namespace gungnir::routing {
class Router;
}

namespace gungnir::view {
class Engine;
}

namespace gungnir::http::detail {

class Server {
public:
    explicit Server(routing::Router& router, RuntimeOptions options = {});
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;

    void listen(std::string host, std::uint16_t port);
    void listen(
        std::string host,
        std::uint16_t port,
        CancellationToken cancellation
    );
    void stop() noexcept;
    void configure(RuntimeOptions options);
    void view_engine(
        std::shared_ptr<view::Engine> engine
    );

    // running() remains true while the server is accepting or draining
    // in-flight cooperative work. accepting() becomes false as soon as
    // stop() begins graceful shutdown.
    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] bool accepting() const noexcept;
    [[nodiscard]] std::size_t active_dispatches() const noexcept;
    [[nodiscard]] std::uint16_t bound_port() const noexcept;
    [[nodiscard]] const RuntimeOptions& options() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::http::detail
