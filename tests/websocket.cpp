#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <gungnir/core/timer.hpp>
#include <gungnir/http/server.hpp>
#include <gungnir/http/websocket.hpp>
#include <gungnir/observability/observability.hpp>
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
constexpr NativeSocket invalid_socket =
    INVALID_SOCKET;

void close_socket(
    NativeSocket socket
) noexcept {
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

void close_socket(
    NativeSocket socket
) noexcept {
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

    [[nodiscard]]
    NativeSocket get()
        const noexcept {
        return socket_;
    }

private:
    NativeSocket socket_;
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
            "Unable to create WebSocket test socket"
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
            "Unable to prepare WebSocket test address"
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
            "Unable to connect WebSocket client"
        );
    }

    return socket;
}

void send_all(
    NativeSocket socket,
    std::string_view data
) {
    std::size_t offset = 0;

    while (offset < data.size()) {
#ifdef _WIN32
        const auto chunk =
            std::min<std::size_t>(
                data.size() - offset,
                static_cast<std::size_t>(
                    std::numeric_limits<int>::
                        max()
                )
            );

        const auto written =
            ::send(
                socket,
                data.data() + offset,
                static_cast<int>(
                    chunk
                ),
                0
            );
#else
        const auto written =
            ::send(
                socket,
                data.data() + offset,
                data.size() - offset,
                0
            );
#endif

        if (written <= 0) {
            throw std::runtime_error(
                "Unable to write WebSocket test data"
            );
        }

        offset +=
            static_cast<std::size_t>(
                written
            );
    }
}

[[nodiscard]]
std::string receive_until(
    NativeSocket socket,
    std::string_view marker
) {
    std::string output;
    char buffer[2048];

    for (
        int attempt = 0;
        attempt < 1000;
        ++attempt
    ) {
        if (
            output.find(marker) !=
            std::string::npos
        ) {
            return output;
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
                "WebSocket connection closed before expected marker"
            );
        }

        output.append(
            buffer,
            static_cast<std::size_t>(
                received
            )
        );
    }

    throw std::runtime_error(
        "Timed out reading WebSocket response"
    );
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
            std::chrono::milliseconds{5}
        );
    }

    throw std::runtime_error(
        "WebSocket server did not bind"
    );
}

[[nodiscard]]
std::string masked_frame(
    gungnir::http::WebSocketOpcode opcode,
    std::string_view payload,
    bool final = true
) {
    assert(payload.size() <= 125);

    constexpr std::array<
        std::uint8_t,
        4
    > mask{
        0x11,
        0x22,
        0x33,
        0x44
    };

    std::string frame;

    frame.push_back(
        static_cast<char>(
            (
                final
                ? 0x80
                : 0
            ) |
            static_cast<std::uint8_t>(
                opcode
            )
        )
    );

    frame.push_back(
        static_cast<char>(
            0x80 |
            payload.size()
        )
    );

    for (const auto value : mask) {
        frame.push_back(
            static_cast<char>(
                value
            )
        );
    }

    for (
        std::size_t index = 0;
        index < payload.size();
        ++index
    ) {
        frame.push_back(
            static_cast<char>(
                static_cast<std::uint8_t>(
                    payload[index]
                ) ^
                mask[index % 4]
            )
        );
    }

    return frame;
}

struct ServerFrame {
    gungnir::http::WebSocketOpcode
        opcode;
    std::string payload;
};

[[nodiscard]]
ServerFrame receive_server_frame(
    NativeSocket socket
) {
    std::string bytes;
    char buffer[512];

    auto ensure =
        [&](std::size_t needed) {
            while (
                bytes.size() < needed
            ) {
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
                        "WebSocket server frame ended early"
                    );
                }

                bytes.append(
                    buffer,
                    static_cast<std::size_t>(
                        received
                    )
                );
            }
        };

    ensure(2);

    const auto first =
        static_cast<std::uint8_t>(
            bytes[0]
        );

    const auto second =
        static_cast<std::uint8_t>(
            bytes[1]
        );

    assert((first & 0x80) != 0);
    assert((second & 0x80) == 0);

    std::uint64_t length =
        second & 0x7f;

    std::size_t cursor = 2;

    if (length == 126) {
        ensure(4);

        length =
            (
                static_cast<std::uint8_t>(
                    bytes[2]
                ) << 8
            ) |
            static_cast<std::uint8_t>(
                bytes[3]
            );

        cursor = 4;
    } else if (
        length == 127
    ) {
        ensure(10);

        length = 0;

        for (
            std::size_t index = 0;
            index < 8;
            ++index
        ) {
            length =
                (
                    length << 8
                ) |
                static_cast<std::uint8_t>(
                    bytes[
                        2 + index
                    ]
                );
        }

        cursor = 10;
    }

    assert(
        length <=
        static_cast<std::uint64_t>(
            std::numeric_limits<
                std::size_t
            >::max()
        )
    );

    ensure(
        cursor +
        static_cast<std::size_t>(
            length
        )
    );

    return {
        static_cast<
            gungnir::http::
                WebSocketOpcode
        >(
            first & 0x0f
        ),
        bytes.substr(
            cursor,
            static_cast<std::size_t>(
                length
            )
        )
    };
}

void websocket_handshake(
    NativeSocket socket
) {
    send_all(
        socket,
        "GET /ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: keep-alive, Upgrade\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Protocol: chat, superchat\r\n"
        "\r\n"
    );

    const auto response =
        receive_until(
            socket,
            "\r\n\r\n"
        );

    assert(
        response.find(
            "HTTP/1.1 101 Switching Protocols\r\n"
        ) == 0
    );

    assert(
        response.find(
            "sec-websocket-accept: "
            "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
        ) !=
        std::string::npos
    );

    assert(
        response.find(
            "sec-websocket-protocol: chat\r\n"
        ) !=
        std::string::npos
    );
}

} // namespace

int main() {
    using namespace std::chrono_literals;
    using namespace gungnir;

    SocketRuntime socket_runtime;

    routing::Router router;

    router.get(
        "/ws",
        [] {
            return
                http::Response::websocket(
                    http::WebSocketSession{
                        http::WebSocketSession::
                            Handler{
                                [](
                                    http::
                                        WebSocketMessage
                                            message
                                )
                                    -> Task<
                                        http::
                                            WebSocketSession::
                                                Reply
                                    > {
                                    co_await sleep_for(
                                        5ms
                                    );

                                    if (
                                        message.opcode ==
                                        http::
                                            WebSocketOpcode::
                                                text
                                    ) {
                                        co_return
                                            http::
                                                WebSocketMessage::
                                                    text(
                                                        "echo:" +
                                                        message.payload
                                                    );
                                    }

                                    co_return
                                        http::
                                            WebSocketMessage::
                                                binary(
                                                    std::move(
                                                        message.payload
                                                    )
                                                );
                                }
                            }
                    },
                    "chat"
                );
        }
    );

    http::RuntimeOptions options;
    options.max_websocket_message_bytes =
        1024;
    options.websocket_message_timeout =
        1s;
    options.write_timeout = 1s;
    options.idle_timeout = 2s;
    options.shutdown_timeout = 1s;

    http::detail::Server server{
        router,
        options
    };

    std::exception_ptr
        server_error;

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

        websocket_handshake(
            client.get()
        );

        send_all(
            client.get(),
            masked_frame(
                http::
                    WebSocketOpcode::text,
                "hel",
                false
            )
        );

        send_all(
            client.get(),
            masked_frame(
                http::
                    WebSocketOpcode::
                        continuation,
                "lo",
                true
            )
        );

        const auto echo =
            receive_server_frame(
                client.get()
            );

        assert(
            echo.opcode ==
            http::
                WebSocketOpcode::text
        );

        assert(
            echo.payload ==
            "echo:hello"
        );

        send_all(
            client.get(),
            masked_frame(
                http::
                    WebSocketOpcode::ping,
                "p"
            )
        );

        const auto pong =
            receive_server_frame(
                client.get()
            );

        assert(
            pong.opcode ==
            http::
                WebSocketOpcode::pong
        );

        assert(pong.payload == "p");

        std::string close_payload;
        close_payload.push_back(
            static_cast<char>(
                1000 >> 8
            )
        );
        close_payload.push_back(
            static_cast<char>(
                1000 & 0xff
            )
        );

        send_all(
            client.get(),
            masked_frame(
                http::
                    WebSocketOpcode::close,
                close_payload
            )
        );

        const auto close =
            receive_server_frame(
                client.get()
            );

        assert(
            close.opcode ==
            http::
                WebSocketOpcode::close
        );

        assert(
            close.payload ==
            close_payload
        );
    }

    {
        SocketGuard client{
            connect_local(port)
        };

        websocket_handshake(
            client.get()
        );

        const auto unmasked =
            http::websocket::wire::
                serialize_server_frame(
                    http::
                        WebSocketOpcode::text,
                    "invalid"
                );

        send_all(
            client.get(),
            unmasked
        );

        const auto close =
            receive_server_frame(
                client.get()
            );

        assert(
            close.opcode ==
            http::
                WebSocketOpcode::close
        );

        assert(close.payload.size() >= 2);

        const auto code =
            static_cast<std::uint16_t>(
                (
                    static_cast<
                        std::uint8_t
                    >(
                        close.payload[0]
                    ) << 8
                ) |
                static_cast<
                    std::uint8_t
                >(
                    close.payload[1]
                )
            );

        assert(code == 1002);
    }

    server.stop();
    server_thread.join();

    if (server_error) {
        std::rethrow_exception(
            server_error
        );
    }

    return 0;
}
