#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <exception>
#include <limits>
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

        if (
            WSAStartup(
                MAKEWORD(2, 2),
                &data
            ) != 0
        ) {
            throw std::runtime_error(
                "Unable to initialize WinSock"
            );
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
    explicit SocketGuard(
        NativeSocket socket
    ) noexcept
        : socket_(socket) {}

    ~SocketGuard() {
        close_socket(socket_);
    }

    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(
        const SocketGuard&
    ) = delete;

    [[nodiscard]]
    NativeSocket get() const noexcept {
        return socket_;
    }

private:
    NativeSocket socket_{invalid_socket};
};

[[nodiscard]]
NativeSocket connect_local(
    std::uint16_t port
) {
    const auto socket =
        ::socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP
        );

    if (socket == invalid_socket) {
        throw std::runtime_error(
            "Unable to create overload test socket"
        );
    }

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
            "Unable to prepare overload test address"
        );
    }

    if (
        ::connect(
            socket,
            reinterpret_cast<
                const sockaddr*
            >(&address),
#ifdef _WIN32
            static_cast<int>(
                sizeof(address)
            )
#else
            static_cast<socklen_t>(
                sizeof(address)
            )
#endif
        ) != 0
    ) {
        close_socket(socket);
        throw std::runtime_error(
            "Unable to connect overload test client"
        );
    }

    return socket;
}

void send_all(
    NativeSocket socket,
    std::string_view data
) {
    std::size_t sent = 0;

    while (sent < data.size()) {
        const auto remaining =
            data.size() - sent;

#ifdef _WIN32
        const auto chunk =
            static_cast<int>(
                std::min<std::size_t>(
                    remaining,
                    static_cast<std::size_t>(
                        std::numeric_limits<int>::
                            max()
                    )
                )
            );

        const auto written =
            ::send(
                socket,
                data.data() + sent,
                chunk,
                0
            );
#else
        const auto written =
            ::send(
                socket,
                data.data() + sent,
                remaining,
                0
            );
#endif

        if (written <= 0) {
            throw std::runtime_error(
                "Unable to write overload test request"
            );
        }

        sent +=
            static_cast<std::size_t>(
                written
            );
    }
}

[[nodiscard]]
std::string receive_all(
    NativeSocket socket
) {
    std::string response;
    char buffer[4096];

    while (true) {
#ifdef _WIN32
        const auto received =
            ::recv(
                socket,
                buffer,
                static_cast<int>(
                    sizeof(buffer)
                ),
                0
            );
#else
        const auto received =
            ::recv(
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
            static_cast<std::size_t>(
                received
            )
        );
    }
}

[[nodiscard]]
std::uint16_t wait_for_port(
    const gungnir::http::detail::Server&
        server
) {
    for (
        int attempt = 0;
        attempt < 400;
        ++attempt
    ) {
        const auto port =
            server.bound_port();

        if (port != 0) {
            return port;
        }

        std::this_thread::sleep_for(
            5ms
        );
    }

    throw std::runtime_error(
        "Overload test server did not bind"
    );
}

} // namespace

int main() {
    using namespace gungnir;

    SocketRuntime socket_runtime;

    routing::Router router;

    std::atomic_bool started{false};
    std::atomic_bool release{false};

    router.get(
        "/hold",
        [&](
            http::Request& request
        ) -> Task<http::Response> {
            started.store(
                true,
                std::memory_order_release
            );

            while (
                !release.load(
                    std::memory_order_acquire
                )
            ) {
                co_await sleep_for(
                    5ms,
                    request.cancellation()
                );
            }

            co_return
                http::Response::text(
                    "released"
                );
        }
    );

    router.get(
        "/fast",
        [] {
            return http::Response::text(
                "fast"
            );
        }
    );

    http::RuntimeOptions options;
    options.max_connections = 8;
    options.max_active_dispatches = 1;
    options.request_timeout = 2s;
    options.read_timeout = 2s;
    options.write_timeout = 2s;
    options.idle_timeout = 2s;
    options.shutdown_timeout = 1s;

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

    const auto port =
        wait_for_port(server);

    SocketGuard first{
        connect_local(port)
    };

    send_all(
        first.get(),
        "GET /hold HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n"
    );

    for (
        int attempt = 0;
        attempt < 400 &&
            !started.load(
                std::memory_order_acquire
            );
        ++attempt
    ) {
        std::this_thread::sleep_for(
            5ms
        );
    }

    assert(
        started.load(
            std::memory_order_acquire
        )
    );

    assert(
        server.active_dispatches() == 1
    );

    {
        SocketGuard second{
            connect_local(port)
        };

        send_all(
            second.get(),
            "GET /fast HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: close\r\n"
            "\r\n"
        );

        const auto response =
            receive_all(
                second.get()
            );

        assert(
            response.find(
                "HTTP/1.1 503 Service Unavailable\r\n"
            ) == 0
        );

        assert(
            response.find(
                "retry-after: 1\r\n"
            ) != std::string::npos
        );

        assert(
            response.ends_with(
                "\r\n\r\nService Unavailable"
            )
        );
    }

    release.store(
        true,
        std::memory_order_release
    );

    const auto released =
        receive_all(
            first.get()
        );

    assert(
        released.find(
            "HTTP/1.1 200 OK\r\n"
        ) == 0
    );

    assert(
        released.ends_with(
            "\r\n\r\nreleased"
        )
    );

    server.stop();
    server_thread.join();

    if (server_error) {
        std::rethrow_exception(
            server_error
        );
    }

    assert(
        server.active_dispatches() == 0
    );

    http::RuntimeOptions invalid =
        options;

    invalid.max_active_dispatches = 0;

    bool rejected = false;

    try {
        http::detail::Server invalid_server{
            router,
            invalid
        };
    } catch (
        const std::invalid_argument&
    ) {
        rejected = true;
    }

    assert(rejected);
}
