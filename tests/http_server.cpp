#include <algorithm>
#include <cassert>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

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
                "Unable to initialize test WinSock"
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
        NativeSocket value
    ) noexcept
        : value_(value) {}

    ~SocketGuard() {
        close_socket(value_);
    }

    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;

    [[nodiscard]]
    NativeSocket get() const noexcept {
        return value_;
    }

private:
    NativeSocket value_;
};

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
            "Unable to create HTTP test socket"
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
            "Unable to prepare HTTP test address"
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
            "Unable to connect to HTTP test server"
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
                "Unable to write HTTP test request"
            );
        }

        sent +=
            static_cast<std::size_t>(
                written
            );
    }
}

std::size_t content_length(
    std::string_view headers
) {
    const auto marker =
        headers.find(
            "content-length:"
        );

    if (
        marker ==
        std::string_view::npos
    ) {
        throw std::runtime_error(
            "HTTP test response has no Content-Length"
        );
    }

    const auto value_begin =
        marker +
        std::string_view{
            "content-length:"
        }.size();

    const auto line_end =
        headers.find(
            "\r\n",
            value_begin
        );

    auto value =
        headers.substr(
            value_begin,
            line_end - value_begin
        );

    while (
        !value.empty() &&
        value.front() == ' '
    ) {
        value.remove_prefix(1);
    }

    std::size_t parsed = 0;

    const auto result =
        std::from_chars(
            value.data(),
            value.data() +
                value.size(),
            parsed
        );

    if (
        result.ec != std::errc{} ||
        result.ptr !=
            value.data() +
                value.size()
    ) {
        throw std::runtime_error(
            "HTTP test response has invalid Content-Length"
        );
    }

    return parsed;
}

std::string receive_response(
    NativeSocket socket
) {
    std::string response;
    char buffer[2048];

    while (true) {
        const auto header_end =
            response.find(
                "\r\n\r\n"
            );

        if (
            header_end !=
            std::string::npos
        ) {
            const auto body_size =
                content_length(
                    std::string_view{
                        response
                    }.substr(
                        0,
                        header_end
                    )
                );

            const auto total =
                header_end +
                4 +
                body_size;

            if (
                response.size() >=
                total
            ) {
                response.resize(
                    total
                );

                return response;
            }
        }

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
            throw std::runtime_error(
                "HTTP test connection closed before a complete response"
            );
        }

        response.append(
            buffer,
            static_cast<std::size_t>(
                received
            )
        );
    }
}

std::uint16_t wait_for_port(
    const gungnir::http::detail::Server&
        server
) {
    for (
        int attempt = 0;
        attempt < 200;
        ++attempt
    ) {
        const auto port =
            server.bound_port();

        if (port != 0) {
            return port;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds{5}
        );
    }

    throw std::runtime_error(
        "HTTP server did not bind a port"
    );
}

} // namespace

int main() {
    using namespace gungnir;

    SocketRuntime socket_runtime;

    routing::Router router;

    router.get(
        "/ping",
        [] {
            return http::Response::text(
                "pong"
            );
        }
    );

    http::RuntimeOptions options;
    options.max_request_bytes = 4096;
    options.max_header_bytes = 256;
    options.max_connections = 16;
    options.max_requests_per_connection = 2;
    options.read_timeout =
        std::chrono::milliseconds{2000};
    options.write_timeout =
        std::chrono::milliseconds{2000};
    options.idle_timeout =
        std::chrono::milliseconds{2000};
    options.shutdown_timeout =
        std::chrono::milliseconds{1000};
    options.keep_alive = true;

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

    {
        SocketGuard client{
            connect_local(port)
        };

        send_all(
            client.get(),
            "GET /ping HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
        );

        const auto first =
            receive_response(
                client.get()
            );

        assert(
            first.find(
                "HTTP/1.1 200 OK\r\n"
            ) == 0
        );

        assert(
            first.find(
                "connection: keep-alive\r\n"
            ) !=
            std::string::npos
        );

        assert(
            first.ends_with(
                "\r\n\r\npong"
            )
        );

        send_all(
            client.get(),
            "GET /ping HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "\r\n"
        );

        const auto second =
            receive_response(
                client.get()
            );

        assert(
            second.find(
                "connection: close\r\n"
            ) !=
            std::string::npos
        );

        assert(
            second.ends_with(
                "\r\n\r\npong"
            )
        );
    }

    {
        SocketGuard client{
            connect_local(port)
        };

        const std::string large_header(
            300,
            'x'
        );

        send_all(
            client.get(),
            "GET /ping HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "X-Large: " +
            large_header +
            "\r\n\r\n"
        );

        const auto response =
            receive_response(
                client.get()
            );

        assert(
            response.find(
                "HTTP/1.1 431 Request Header Fields Too Large\r\n"
            ) == 0
        );

        assert(
            response.find(
                "connection: close\r\n"
            ) !=
            std::string::npos
        );
    }

    server.stop();
    server_thread.join();

    if (server_error) {
        std::rethrow_exception(
            server_error
        );
    }

    assert(!server.running());

    return 0;
}
