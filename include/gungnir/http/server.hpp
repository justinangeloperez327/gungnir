#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace gungnir::routing {
class Router;
}

namespace gungnir::http::detail {

class Server {
public:
    explicit Server(routing::Router& router);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;

    void listen(std::string host, std::uint16_t port);
    void stop() noexcept;

    [[nodiscard]] bool running() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gungnir::http::detail
