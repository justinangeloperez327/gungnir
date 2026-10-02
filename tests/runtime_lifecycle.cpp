#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <gungnir/core/timer.hpp>
#include <gungnir/http/server.hpp>
#include <gungnir/routing/router.hpp>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {

using namespace std::chrono_literals;

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket invalid_socket = INVALID_SOCKET;

void close_socket(NativeSocket socket) noexcept {
    if (socket != invalid_socket) {
        closesocket(socket);
    }
}

class SocketRuntime {
public:
    SocketRuntime() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("Unable to initialize WinSock");
        }
    }

    ~SocketRuntime() {
        WSACleanup();
    }
};
#else
using NativeSocket = int;
constexpr NativeSocket invalid_socket = -1;

void close_socket(NativeSocket socket) noexcept {
    if (socket != invalid_socket) {
        ::close(socket);
    }
}

class SocketRuntime {};
#endif

class SocketGuard {
public:
    explicit SocketGuard(NativeSocket socket) noexcept
        : socket_(socket) {}

    ~SocketGuard() {
        close_socket(socket_);
    }

    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;

    [[nodiscard]]
    NativeSocket get() const noexcept {
        return socket_;
    }

private:
    NativeSocket socket_{invalid_socket};
};

[[nodiscard]]
NativeSocket connect_local(std::uint16_t port) {
    const auto socket = ::socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (socket == invalid_socket) {
        throw std::runtime_error(
            "Unable to create runtime lifecycle socket"
        );
    }

#ifdef _WIN32
    const DWORD timeout = 3000;
    static_cast<void>(
        setsockopt(
            socket,
            SOL_SOCKET,
            SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeout),
            static_cast<int>(sizeof(timeout))
        )
    );
#else
    timeval timeout{};
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    static_cast<void>(
        setsockopt(
            socket,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &timeout,
            static_cast<socklen_t>(sizeof(timeout))
        )
    );
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (
        inet_pton(
            AF_INET,
            "127.0.0.1",
            &address.sin_addr
        ) != 1
    ) {
        close_socket(socket);
        throw std::runtime_error(
            "Unable to prepare runtime lifecycle address"
        );
    }

    if (
        ::connect(
            socket,
            reinterpret_cast<const sockaddr*>(&address),
#ifdef _WIN32
            static_cast<int>(sizeof(address))
#else
            static_cast<socklen_t>(sizeof(address))
#endif
        ) != 0
    ) {
        close_socket(socket);
        throw std::runtime_error(
            "Unable to connect runtime lifecycle client"
        );
    }

    return socket;
}

void send_all(
    NativeSocket socket,
    std::string_view value
) {
    std::size_t offset = 0;

    while (offset < value.size()) {
#ifdef _WIN32
        const auto written = ::send(
            socket,
            value.data() + offset,
            static_cast<int>(value.size() - offset),
            0
        );
#else
        const auto written = ::send(
            socket,
            value.data() + offset,
            value.size() - offset,
            0
        );
#endif

        if (written <= 0) {
            throw std::runtime_error(
                "Unable to write runtime lifecycle request"
            );
        }

        offset += static_cast<std::size_t>(written);
    }
}

[[nodiscard]]
std::string receive_all(NativeSocket socket) {
    std::string response;
    char buffer[4096];

    while (true) {
#ifdef _WIN32
        const auto received = ::recv(
            socket,
            buffer,
            static_cast<int>(sizeof(buffer)),
            0
        );
#else
        const auto received = ::recv(
            socket,
            buffer,
            sizeof(buffer),
            0
        );
#endif

        if (received <= 0) {
            return response;
        }

        response.append(
            buffer,
            static_cast<std::size_t>(received)
        );
    }
}

[[nodiscard]]
std::uint16_t wait_for_port(
    const gungnir::http::detail::Server& server
) {
    for (int attempt = 0; attempt < 400; ++attempt) {
        const auto port = server.bound_port();

        if (port != 0) {
            return port;
        }

        std::this_thread::sleep_for(5ms);
    }

    throw std::runtime_error(
        "Runtime lifecycle server did not bind"
    );
}

void wait_for(
    const std::atomic_bool& value,
    std::string_view message
) {
    for (int attempt = 0; attempt < 400; ++attempt) {
        if (value.load(std::memory_order_acquire)) {
            return;
        }

        std::this_thread::sleep_for(5ms);
    }

    throw std::runtime_error(std::string{message});
}

void graceful_drain_keeps_runtime_active() {
    using namespace gungnir;

    routing::Router router;
    std::atomic_bool started{false};

    router.get(
        "/drain",
        [&](http::Request& request)
            -> Task<http::Response> {
            started.store(
                true,
                std::memory_order_release
            );

            co_await sleep_for(
                150ms,
                request.cancellation()
            );

            co_return http::Response::text(
                "drained"
            );
        }
    );

    http::RuntimeOptions options;
    options.request_timeout = 2s;
    options.shutdown_timeout = 1s;
    options.read_timeout = 2s;
    options.write_timeout = 2s;
    options.idle_timeout = 2s;

    http::detail::Server server{
        router,
        options
    };

    std::exception_ptr server_error;

    std::thread server_thread{
        [&] {
            try {
                server.listen(
                    "127.0.0.1",
                    0
                );
            } catch (...) {
                server_error =
                    std::current_exception();
            }
        }
    };

    const auto port = wait_for_port(server);

    SocketGuard client{
        connect_local(port)
    };

    send_all(
        client.get(),
        "GET /drain HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n"
    );

    wait_for(
        started,
        "Drain handler did not start"
    );

    assert(server.running());
    assert(server.accepting());
    assert(server.active_dispatches() >= 1);

    server.stop();

    assert(server.running());
    assert(!server.accepting());
    assert(server.active_dispatches() >= 1);

    bool concurrent_listen_rejected = false;

    try {
        server.listen(
            "127.0.0.1",
            0
        );
    } catch (
        const std::logic_error&
    ) {
        concurrent_listen_rejected = true;
    }

    assert(concurrent_listen_rejected);

    const auto response =
        receive_all(client.get());

    assert(
        response.find(
            "HTTP/1.1 200 OK\r\n"
        ) == 0
    );

    assert(
        response.ends_with(
            "\r\n\r\ndrained"
        )
    );

    server_thread.join();

    if (server_error) {
        std::rethrow_exception(
            server_error
        );
    }

    assert(!server.running());
    assert(!server.accepting());
    assert(server.active_dispatches() == 0);
}

void shutdown_deadline_cancels_request() {
    using namespace gungnir;

    routing::Router router;
    std::atomic_bool started{false};
    std::atomic_bool cancelled{false};

    router.get(
        "/cancel",
        [&](http::Request& request)
            -> Task<http::Response> {
            started.store(
                true,
                std::memory_order_release
            );

            try {
                co_await sleep_for(
                    5s,
                    request.cancellation()
                );
            } catch (
                const OperationCancelled&
            ) {
                cancelled.store(
                    true,
                    std::memory_order_release
                );

                throw;
            }

            co_return http::Response::text(
                "late"
            );
        }
    );

    http::RuntimeOptions options;
    options.request_timeout = 5s;
    options.shutdown_timeout = 75ms;
    options.read_timeout = 2s;
    options.write_timeout = 2s;
    options.idle_timeout = 2s;

    http::detail::Server server{
        router,
        options
    };

    std::exception_ptr server_error;

    std::thread server_thread{
        [&] {
            try {
                server.listen(
                    "127.0.0.1",
                    0
                );
            } catch (...) {
                server_error =
                    std::current_exception();
            }
        }
    };

    const auto port = wait_for_port(server);

    SocketGuard client{
        connect_local(port)
    };

    send_all(
        client.get(),
        "GET /cancel HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n"
    );

    wait_for(
        started,
        "Cancellation handler did not start"
    );

    const auto stop_started =
        std::chrono::steady_clock::now();

    server.stop();

    assert(server.running());
    assert(!server.accepting());

    server_thread.join();

    const auto elapsed =
        std::chrono::steady_clock::now() -
        stop_started;

    if (server_error) {
        std::rethrow_exception(
            server_error
        );
    }

    assert(
        cancelled.load(
            std::memory_order_acquire
        )
    );

    assert(elapsed >= 40ms);
    assert(elapsed < 1s);
    assert(!server.running());
    assert(!server.accepting());
    assert(server.active_dispatches() == 0);
}

} // namespace

int main() {
    SocketRuntime socket_runtime;

    graceful_drain_keeps_runtime_active();
    shutdown_deadline_cancels_request();
}
