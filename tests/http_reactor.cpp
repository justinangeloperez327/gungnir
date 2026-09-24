#include <cassert>
#include <chrono>
#include <cstdint>
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
#else
using NativeSocket = int;
constexpr NativeSocket invalid_socket = -1;

void close_socket(NativeSocket socket) noexcept {
    if (socket != invalid_socket) {
        ::close(socket);
    }
}
#endif

class ClientSocket {
public:
    explicit ClientSocket(
        std::uint16_t port
    ) {
        socket_ = ::socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP
        );

        if (
            socket_ ==
            invalid_socket
        ) {
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
            close_socket(socket_);
            socket_ = invalid_socket;

            throw std::runtime_error(
                "Unable to parse HTTP test address"
            );
        }

        if (
            ::connect(
                socket_,
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
            close_socket(socket_);
            socket_ = invalid_socket;

            throw std::runtime_error(
                "Unable to connect HTTP test socket"
            );
        }
    }

    ~ClientSocket() {
        close_socket(socket_);
    }

    ClientSocket(
        const ClientSocket&
    ) = delete;

    ClientSocket& operator=(
        const ClientSocket&
    ) = delete;

    void send(
        std::string_view value
    ) {
        std::size_t offset = 0;

        while (
            offset <
            value.size()
        ) {
            const auto remaining =
                value.size() - offset;

#ifdef _WIN32
            const auto written =
                ::send(
                    socket_,
                    value.data() + offset,
                    static_cast<int>(
                        remaining
                    ),
                    0
                );
#else
            const auto written =
                ::send(
                    socket_,
                    value.data() + offset,
                    remaining,
                    0
                );
#endif

            if (written <= 0) {
                throw std::runtime_error(
                    "Unable to send HTTP test request"
                );
            }

            offset +=
                static_cast<std::size_t>(
                    written
                );
        }
    }

    std::string receive_response() {
        std::string response;
        char buffer[4096];

        while (true) {
#ifdef _WIN32
            const auto received =
                ::recv(
                    socket_,
                    buffer,
                    static_cast<int>(
                        sizeof(buffer)
                    ),
                    0
                );
#else
            const auto received =
                ::recv(
                    socket_,
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

            const auto header_end =
                response.find(
                    "\r\n\r\n"
                );

            if (
                header_end ==
                std::string::npos
            ) {
                continue;
            }

            const auto length_header =
                response.find(
                    "content-length: "
                );

            if (
                length_header ==
                std::string::npos ||
                length_header >
                    header_end
            ) {
                throw std::runtime_error(
                    "HTTP test response has no content length"
                );
            }

            const auto value_start =
                length_header +
                std::string_view{
                    "content-length: "
                }.size();

            const auto value_end =
                response.find(
                    "\r\n",
                    value_start
                );

            const auto content_length =
                static_cast<std::size_t>(
                    std::stoull(
                        response.substr(
                            value_start,
                            value_end -
                                value_start
                        )
                    )
                );

            const auto expected =
                header_end + 4 +
                content_length;

            if (
                response.size() >=
                expected
            ) {
                response.resize(
                    expected
                );

                return response;
            }
        }
    }

    [[nodiscard]]
    bool closed() {
        char value = 0;

#ifdef _WIN32
        const auto received =
            ::recv(
                socket_,
                &value,
                1,
                0
            );
#else
        const auto received =
            ::recv(
                socket_,
                &value,
                1,
                0
            );
#endif

        return received == 0;
    }

private:
    NativeSocket socket_{
        invalid_socket
    };
};

class RunningServer {
public:
    RunningServer(
        gungnir::routing::Router& router,
        gungnir::http::RuntimeOptions options
    )
        : server_(
            router,
            std::move(options)
          ),
          thread_(
            [this] {
                server_.listen(
                    "127.0.0.1",
                    0
                );
            }
          ) {
        for (
            int attempt = 0;
            attempt < 200;
            ++attempt
        ) {
            if (
                server_.bound_port() !=
                0
            ) {
                return;
            }

            std::this_thread::sleep_for(
                5ms
            );
        }

        server_.stop();
        thread_.join();

        throw std::runtime_error(
            "HTTP test server did not bind"
        );
    }

    ~RunningServer() {
        server_.stop();

        if (
            thread_.joinable()
        ) {
            thread_.join();
        }
    }

    [[nodiscard]]
    std::uint16_t port()
        const noexcept {
        return server_.bound_port();
    }

private:
    gungnir::http::detail::Server server_;
    std::thread thread_;
};

std::string request(
    std::string_view connection
) {
    return
        "GET /ping HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: " +
        std::string{connection} +
        "\r\n\r\n";
}

} // namespace

int main() {
    using namespace gungnir;

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
    options.max_request_bytes = 1024;
    options.max_header_bytes = 256;
    options.max_connections = 8;
    options.max_requests_per_connection = 2;
    options.read_timeout = 2s;
    options.write_timeout = 2s;
    options.idle_timeout = 2s;
    options.shutdown_timeout = 1s;
    options.keep_alive = true;

    RunningServer server{
        router,
        options
    };

    {
        ClientSocket client{
            server.port()
        };

        client.send(
            request(
                "keep-alive"
            )
        );

        const auto first =
            client.receive_response();

        assert(
            first.starts_with(
                "HTTP/1.1 200 OK\r\n"
            )
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

        client.send(
            request(
                "keep-alive"
            )
        );

        const auto second =
            client.receive_response();

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

        assert(
            client.closed()
        );
    }

    {
        ClientSocket client{
            server.port()
        };

        std::string oversized =
            "GET /ping HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "X-Large: ";

        oversized.append(
            300,
            'a'
        );

        oversized +=
            "\r\n\r\n";

        client.send(
            oversized
        );

        const auto response =
            client.receive_response();

        assert(
            response.starts_with(
                "HTTP/1.1 431 Request Header Fields Too Large\r\n"
            )
        );

        assert(
            response.find(
                "connection: close\r\n"
            ) !=
            std::string::npos
        );
    }

    return 0;
}
