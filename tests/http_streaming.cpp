#include <algorithm>
#include <atomic>
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
            "Unable to create streaming test socket"
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
            "Unable to prepare streaming test address"
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
            "Unable to connect streaming client"
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
        const auto chunk =
            std::min<std::size_t>(
                value.size() - offset,
                static_cast<std::size_t>(
                    std::numeric_limits<int>::
                        max()
                )
            );

        const auto written =
            ::send(
                socket,
                value.data() + offset,
                static_cast<int>(
                    chunk
                ),
                0
            );
#else
        const auto written =
            ::send(
                socket,
                value.data() + offset,
                value.size() - offset,
                0
            );
#endif

        if (written <= 0) {
            throw std::runtime_error(
                "Unable to write streaming request"
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
    std::string response;
    char buffer[2048];

    for (
        int attempt = 0;
        attempt < 2000;
        ++attempt
    ) {
        if (
            response.find(marker) !=
            std::string::npos
        ) {
            return response;
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
                "Streaming connection closed before marker"
            );
        }

        response.append(
            buffer,
            static_cast<std::size_t>(
                received
            )
        );
    }

    throw std::runtime_error(
        "Timed out reading streaming response"
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
        "Streaming server did not bind"
    );
}

} // namespace

int main() {
    using namespace std::chrono_literals;
    using namespace gungnir;

    SocketRuntime socket_runtime;

    routing::Router router;

    auto produced =
        std::make_shared<
            std::atomic_size_t
        >(0);

    router.get(
        "/stream",
        [produced] {
            auto stream =
                http::BodyStream{
                    http::BodyStream::
                        Producer{
                            [produced]()
                                -> Task<
                                    http::BodyStream::
                                        Chunk
                                > {
                                const auto index =
                                    produced
                                        ->fetch_add(
                                            1,
                                            std::memory_order_acq_rel
                                        );

                                if (index >= 3) {
                                    co_return
                                        std::nullopt;
                                }

                                co_await sleep_for(
                                    5ms
                                );

                                static constexpr
                                    std::string_view
                                    values[] = {
                                        "one",
                                        "two",
                                        "three"
                                    };

                                co_return
                                    std::string{
                                        values[index]
                                    };
                            }
                        }
                };

            return
                http::Response::stream(
                    std::move(stream),
                    "text/plain; charset=utf-8"
                );
        }
    );

    router.get(
        "/ping",
        [] {
            return
                http::Response::text(
                    "pong"
                );
        }
    );

    http::RuntimeOptions options;
    options.max_stream_chunk_bytes = 8;
    options.stream_chunk_timeout = 1s;
    options.write_timeout = 1s;
    options.idle_timeout = 1s;
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

        send_all(
            client.get(),
            "GET /stream HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
        );

        const auto streamed =
            receive_until(
                client.get(),
                "0\r\n\r\n"
            );

        assert(
            streamed.find(
                "HTTP/1.1 200 OK\r\n"
            ) == 0
        );

        assert(
            streamed.find(
                "transfer-encoding: chunked\r\n"
            ) !=
            std::string::npos
        );

        assert(
            streamed.find(
                "content-length:"
            ) ==
            std::string::npos
        );

        assert(
            streamed.find(
                "\r\n\r\n"
                "3\r\none\r\n"
                "3\r\ntwo\r\n"
                "5\r\nthree\r\n"
                "0\r\n\r\n"
            ) !=
            std::string::npos
        );

        assert(
            produced->load(
                std::memory_order_acquire
            ) == 4
        );

        send_all(
            client.get(),
            "GET /ping HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Connection: close\r\n"
            "\r\n"
        );

        const auto ping =
            receive_until(
                client.get(),
                "\r\n\r\npong"
            );

        assert(
            ping.find(
                "HTTP/1.1 200 OK\r\n"
            ) == 0
        );

        assert(
            ping.ends_with(
                "\r\n\r\npong"
            )
        );
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
