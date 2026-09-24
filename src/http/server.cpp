#include <gungnir/http/server.hpp>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <gungnir/http/message.hpp>
#include <gungnir/routing/router.hpp>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace gungnir::http::detail {

namespace {

constexpr int listen_backlog = 256;
constexpr int reactor_poll_timeout_ms = 100;

class HeaderLimitError final :
    public std::length_error {
public:
    HeaderLimitError()
        : std::length_error(
            "HTTP request headers are too large"
          ) {}
};

#ifdef _WIN32
using NativeSocket = SOCKET;
using PollFd = WSAPOLLFD;
constexpr NativeSocket invalid_socket =
    INVALID_SOCKET;
constexpr short poll_read_event =
    POLLRDNORM;
constexpr short poll_write_event =
    POLLWRNORM;

void close_socket(
    NativeSocket socket
) noexcept {
    if (socket != invalid_socket) {
        closesocket(socket);
    }
}

bool set_non_blocking(
    NativeSocket socket
) noexcept {
    u_long enabled = 1;

    return
        ioctlsocket(
            socket,
            FIONBIO,
            &enabled
        ) == 0;
}

int socket_error() noexcept {
    return WSAGetLastError();
}

bool would_block(
    int error
) noexcept {
    return error == WSAEWOULDBLOCK;
}

bool interrupted(
    int error
) noexcept {
    return error == WSAEINTR;
}

int poll_sockets(
    PollFd* descriptors,
    std::size_t count,
    int timeout
) {
    if (
        count >
        static_cast<std::size_t>(
            std::numeric_limits<ULONG>::max()
        )
    ) {
        throw std::length_error(
            "HTTP reactor descriptor count exceeds WSAPoll capacity"
        );
    }

    return WSAPoll(
        descriptors,
        static_cast<ULONG>(count),
        timeout
    );
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
using PollFd = pollfd;
constexpr NativeSocket invalid_socket = -1;
constexpr short poll_read_event = POLLIN;
constexpr short poll_write_event = POLLOUT;

void close_socket(
    NativeSocket socket
) noexcept {
    if (socket != invalid_socket) {
        ::close(socket);
    }
}

bool set_non_blocking(
    NativeSocket socket
) noexcept {
    const auto flags =
        fcntl(
            socket,
            F_GETFL,
            0
        );

    if (flags < 0) {
        return false;
    }

    return
        fcntl(
            socket,
            F_SETFL,
            flags | O_NONBLOCK
        ) == 0;
}

int socket_error() noexcept {
    return errno;
}

bool would_block(
    int error
) noexcept {
    return
        error == EAGAIN ||
        error == EWOULDBLOCK;
}

bool interrupted(
    int error
) noexcept {
    return error == EINTR;
}

int poll_sockets(
    PollFd* descriptors,
    std::size_t count,
    int timeout
) {
    return ::poll(
        descriptors,
        static_cast<nfds_t>(count),
        timeout
    );
}

class SocketRuntime {};
#endif

using Clock =
    std::chrono::steady_clock;

std::string_view trim(
    std::string_view value
) {
    while (
        !value.empty() &&
        (
            value.front() == ' ' ||
            value.front() == '\t'
        )
    ) {
        value.remove_prefix(1);
    }

    while (
        !value.empty() &&
        (
            value.back() == ' ' ||
            value.back() == '\t'
        )
    ) {
        value.remove_suffix(1);
    }

    return value;
}

std::string lowercase(
    std::string_view value
) {
    std::string result{value};

    for (auto& character : result) {
        if (
            character >= 'A' &&
            character <= 'Z'
        ) {
            character =
                static_cast<char>(
                    character - 'A' + 'a'
                );
        }
    }

    return result;
}

std::size_t request_content_length(
    std::string_view headers
) {
    std::size_t cursor = 0;

    while (cursor < headers.size()) {
        const auto line_end =
            headers.find(
                "\r\n",
                cursor
            );

        const auto end =
            line_end ==
                std::string_view::npos
            ? headers.size()
            : line_end;

        const auto line =
            headers.substr(
                cursor,
                end - cursor
            );

        const auto colon =
            line.find(':');

        if (
            colon !=
            std::string_view::npos
        ) {
            const auto name =
                lowercase(
                    trim(
                        line.substr(
                            0,
                            colon
                        )
                    )
                );

            const auto value =
                trim(
                    line.substr(
                        colon + 1
                    )
                );

            if (
                name ==
                "content-length"
            ) {
                std::size_t parsed = 0;

                const auto result =
                    std::from_chars(
                        value.data(),
                        value.data() +
                            value.size(),
                        parsed
                    );

                if (
                    result.ec !=
                        std::errc{} ||
                    result.ptr !=
                        value.data() +
                            value.size()
                ) {
                    throw std::invalid_argument(
                        "Invalid Content-Length"
                    );
                }

                return parsed;
            }
        }

        if (
            line_end ==
            std::string_view::npos
        ) {
            break;
        }

        cursor = line_end + 2;
    }

    return 0;
}

std::optional<std::size_t>
request_size(
    const std::string& buffer,
    const RuntimeOptions& options
) {
    const auto header_end =
        buffer.find(
            "\r\n\r\n"
        );

    if (
        header_end ==
        std::string::npos
    ) {
        if (
            buffer.size() >
            options.max_header_bytes
        ) {
            throw HeaderLimitError{};
        }

        if (
            buffer.size() >
            options.max_request_bytes
        ) {
            throw std::length_error(
                "HTTP request is too large"
            );
        }

        return std::nullopt;
    }

    const auto header_size =
        header_end + 4;

    if (
        header_size >
        options.max_header_bytes
    ) {
        throw HeaderLimitError{};
    }

    const auto body_size =
        request_content_length(
            std::string_view{
                buffer
            }.substr(
                0,
                header_end
            )
        );

    if (
        header_size >
            options.max_request_bytes ||
        body_size >
            options.max_request_bytes -
                header_size
    ) {
        throw std::length_error(
            "HTTP request is too large"
        );
    }

    const auto expected =
        header_size + body_size;

    if (
        buffer.size() <
        expected
    ) {
        return std::nullopt;
    }

    return expected;
}

bool raw_http_1_0(
    std::string_view raw
) noexcept {
    const auto line_end =
        raw.find(
            "\r\n"
        );

    if (
        line_end ==
        std::string_view::npos
    ) {
        return false;
    }

    return
        raw.substr(
            0,
            line_end
        ).ends_with(
            " HTTP/1.0"
        );
}

bool keep_alive_for(
    std::string_view raw,
    const Request& request,
    const RuntimeOptions& options,
    std::size_t served
) {
    if (
        !options.keep_alive ||
        served >=
            options.max_requests_per_connection
    ) {
        return false;
    }

    const auto connection =
        lowercase(
            request.header(
                "connection"
            )
        );

    if (
        raw_http_1_0(raw)
    ) {
        return
            connection ==
            "keep-alive";
    }

    return connection != "close";
}

NativeSocket make_listener(
    const std::string& host,
    std::uint16_t port
) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags =
        host.empty()
        ? AI_PASSIVE
        : 0;

    addrinfo* addresses = nullptr;

    const auto service =
        std::to_string(port);

    const auto status =
        getaddrinfo(
            host.empty()
                ? nullptr
                : host.c_str(),
            service.c_str(),
            &hints,
            &addresses
        );

    if (
        status != 0 ||
        addresses == nullptr
    ) {
        throw std::runtime_error(
            "Unable to resolve HTTP listen address"
        );
    }

    NativeSocket listener =
        invalid_socket;

    for (
        auto* current = addresses;
        current != nullptr;
        current = current->ai_next
    ) {
        const auto candidate =
            ::socket(
                current->ai_family,
                current->ai_socktype,
                current->ai_protocol
            );

        if (
            candidate ==
            invalid_socket
        ) {
            continue;
        }

        int enabled = 1;

#ifdef _WIN32
        static_cast<void>(
            setsockopt(
                candidate,
                SOL_SOCKET,
                SO_REUSEADDR,
                reinterpret_cast<
                    const char*
                >(&enabled),
                static_cast<int>(
                    sizeof(enabled)
                )
            )
        );
#else
        static_cast<void>(
            setsockopt(
                candidate,
                SOL_SOCKET,
                SO_REUSEADDR,
                &enabled,
                static_cast<socklen_t>(
                    sizeof(enabled)
                )
            )
        );
#endif

        if (
            ::bind(
                candidate,
                current->ai_addr,
#ifdef _WIN32
                static_cast<int>(
                    current->ai_addrlen
                )
#else
                current->ai_addrlen
#endif
            ) == 0 &&
            ::listen(
                candidate,
                listen_backlog
            ) == 0 &&
            set_non_blocking(
                candidate
            )
        ) {
            listener = candidate;
            break;
        }

        close_socket(
            candidate
        );
    }

    freeaddrinfo(
        addresses
    );

    if (
        listener ==
        invalid_socket
    ) {
        throw std::runtime_error(
            "Unable to bind non-blocking HTTP listener"
        );
    }

    return listener;
}

std::uint16_t socket_port(
    NativeSocket socket
) {
    sockaddr_storage address{};

#ifdef _WIN32
    int length =
        static_cast<int>(
            sizeof(address)
        );
#else
    socklen_t length =
        static_cast<socklen_t>(
            sizeof(address)
        );
#endif

    if (
        getsockname(
            socket,
            reinterpret_cast<
                sockaddr*
            >(&address),
            &length
        ) != 0
    ) {
        throw std::runtime_error(
            "Unable to inspect HTTP listener address"
        );
    }

    if (
        address.ss_family ==
        AF_INET
    ) {
        const auto* value =
            reinterpret_cast<
                const sockaddr_in*
            >(&address);

        return ntohs(
            value->sin_port
        );
    }

    if (
        address.ss_family ==
        AF_INET6
    ) {
        const auto* value =
            reinterpret_cast<
                const sockaddr_in6*
            >(&address);

        return ntohs(
            value->sin6_port
        );
    }

    throw std::runtime_error(
        "Unsupported HTTP listener address family"
    );
}

template <typename T>
T complete_inline(
    Task<T> task
) {
    task.run_inline();

    if (!task.done()) {
        throw std::logic_error(
            "HTTP handler suspended; asynchronous dispatch integration is not active yet"
        );
    }

    auto awaiter =
        task.operator co_await();

    return awaiter.await_resume();
}

Response error_response(
    int status,
    std::string body
) {
    return Response::text(
        std::move(body),
        status
    );
}

struct ConnectionState {
    NativeSocket socket{invalid_socket};
    std::string input;
    std::string output;
    std::size_t output_offset{0};
    std::size_t requests_served{0};
    Clock::time_point phase_started{
        Clock::now()
    };
    Clock::time_point last_activity{
        Clock::now()
    };
    bool close_after_write{false};
    bool closing{false};
};

void close_connection(
    ConnectionState& connection
) noexcept {
    close_socket(
        connection.socket
    );

    connection.socket =
        invalid_socket;

    connection.closing = true;
}

} // namespace

class Server::Impl {
public:
    explicit Impl(
        routing::Router& value,
        RuntimeOptions value_options
    )
        : router(value),
          options(
            std::move(
                value_options
            )
          ) {
        validate_options();
    }

    ~Impl() {
        stop();
        close_all();
    }

    void listen(
        std::string host,
        std::uint16_t port
    ) {
        bool expected = false;

        if (
            !running.compare_exchange_strong(
                expected,
                true
            )
        ) {
            throw std::logic_error(
                "Gungnir HTTP server is already running"
            );
        }

        bound.store(0);

        try {
            const auto socket =
                make_listener(
                    host,
                    port
                );

            listener.store(socket);
            bound.store(
                socket_port(socket)
            );

            reactor_loop();
        } catch (...) {
            running.store(false);

            const auto socket =
                listener.exchange(
                    invalid_socket
                );

            close_socket(socket);
            bound.store(0);
            close_all();
            throw;
        }

        const auto socket =
            listener.exchange(
                invalid_socket
            );

        close_socket(socket);
        close_all();
        running.store(false);
    }

    void stop() noexcept {
        running.store(false);
    }

    void reactor_loop() {
        std::optional<
            Clock::time_point
        > drain_deadline;

        while (true) {
            const auto now =
                Clock::now();

            if (
                !running.load() &&
                !drain_deadline
            ) {
                const auto socket =
                    listener.exchange(
                        invalid_socket
                    );

                close_socket(socket);

                drain_deadline =
                    now +
                    options.shutdown_timeout;

                for (
                    auto& connection :
                    connections
                ) {
                    if (
                        connection.output.empty() &&
                        connection.input.empty()
                    ) {
                        close_connection(
                            connection
                        );
                    } else {
                        connection.close_after_write =
                            true;
                    }
                }
            }

            remove_closed();

            if (
                !running.load() &&
                connections.empty()
            ) {
                return;
            }

            if (
                drain_deadline &&
                now >= *drain_deadline
            ) {
                close_all();
                return;
            }

            expire_connections(now);
            remove_closed();

            std::vector<PollFd> descriptors;
            descriptors.reserve(
                connections.size() + 1
            );

            const auto active_listener =
                listener.load();

            const bool poll_listener =
                running.load() &&
                active_listener !=
                    invalid_socket &&
                connections.size() <
                    options.max_connections;

            if (poll_listener) {
                descriptors.push_back(
                    PollFd{
                        active_listener,
                        poll_read_event,
                        0
                    }
                );
            }

            for (
                const auto& connection :
                connections
            ) {
                short events = 0;

                if (
                    connection.output.empty()
                ) {
                    events =
                        poll_read_event;
                } else {
                    events =
                        poll_write_event;
                }

                descriptors.push_back(
                    PollFd{
                        connection.socket,
                        events,
                        0
                    }
                );
            }

            if (
                descriptors.empty()
            ) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds{
                        reactor_poll_timeout_ms
                    }
                );
                continue;
            }

            const auto status =
                poll_sockets(
                    descriptors.data(),
                    descriptors.size(),
                    reactor_poll_timeout_ms
                );

            if (status < 0) {
                const auto error =
                    socket_error();

                if (
                    interrupted(error)
                ) {
                    continue;
                }

                if (!running.load()) {
                    continue;
                }

                throw std::runtime_error(
                    "HTTP socket readiness polling failed"
                );
            }

            if (status == 0) {
                continue;
            }

            std::size_t offset = 0;

            if (poll_listener) {
                const auto events =
                    descriptors.front()
                        .revents;

                if (
                    (
                        events &
                        poll_read_event
                    ) != 0
                ) {
                    accept_ready();
                }

                offset = 1;
            }

            const auto count =
                std::min(
                    connections.size(),
                    descriptors.size() -
                        offset
                );

            for (
                std::size_t index = 0;
                index < count;
                ++index
            ) {
                auto& connection =
                    connections[index];

                const auto events =
                    descriptors[
                        index + offset
                    ].revents;

                if (
                    (
                        events &
                        (
                            POLLERR |
                            POLLHUP |
                            POLLNVAL
                        )
                    ) != 0
                ) {
                    close_connection(
                        connection
                    );
                    continue;
                }

                if (
                    connection.output.empty() &&
                    (
                        events &
                        poll_read_event
                    ) != 0
                ) {
                    read_ready(
                        connection
                    );
                }

                if (
                    !connection.closing &&
                    !connection.output.empty() &&
                    (
                        events &
                        poll_write_event
                    ) != 0
                ) {
                    write_ready(
                        connection
                    );
                }
            }

            remove_closed();
        }
    }

    void accept_ready() {
        while (
            running.load() &&
            connections.size() <
                options.max_connections
        ) {
            const auto active =
                listener.load();

            if (
                active ==
                invalid_socket
            ) {
                return;
            }

            const auto client =
                ::accept(
                    active,
                    nullptr,
                    nullptr
                );

            if (
                client ==
                invalid_socket
            ) {
                const auto error =
                    socket_error();

                if (
                    interrupted(error)
                ) {
                    continue;
                }

                if (
                    would_block(error)
                ) {
                    return;
                }

                if (!running.load()) {
                    return;
                }

                throw std::runtime_error(
                    "HTTP accept failed"
                );
            }

            if (
                !set_non_blocking(
                    client
                )
            ) {
                close_socket(client);
                continue;
            }

            const auto now =
                Clock::now();

            ConnectionState connection;
            connection.socket = client;
            connection.input.reserve(
                std::min<std::size_t>(
                    options.max_request_bytes,
                    8192
                )
            );
            connection.phase_started =
                now;
            connection.last_activity =
                now;

            connections.push_back(
                std::move(connection)
            );
        }
    }

    void read_ready(
        ConnectionState& connection
    ) noexcept {
        try {
            char buffer[8192];

            while (true) {
#ifdef _WIN32
                const auto received =
                    ::recv(
                        connection.socket,
                        buffer,
                        static_cast<int>(
                            sizeof(buffer)
                        ),
                        0
                    );
#else
                const auto received =
                    ::recv(
                        connection.socket,
                        buffer,
                        sizeof(buffer),
                        0
                    );
#endif

                if (received > 0) {
                    const auto count =
                        static_cast<std::size_t>(
                            received
                        );

                    if (
                        connection.input.size() >
                        options.max_request_bytes -
                            std::min(
                                options.max_request_bytes,
                                count
                            )
                    ) {
                        queue_error(
                            connection,
                            413,
                            "Payload Too Large"
                        );
                        return;
                    }

                    connection.input.append(
                        buffer,
                        count
                    );

                    connection.last_activity =
                        Clock::now();

                    continue;
                }

                if (received == 0) {
                    close_connection(
                        connection
                    );
                    return;
                }

                const auto error =
                    socket_error();

                if (
                    interrupted(error)
                ) {
                    continue;
                }

                if (
                    would_block(error)
                ) {
                    break;
                }

                close_connection(
                    connection
                );
                return;
            }

            prepare_response(
                connection
            );
        } catch (
            const HeaderLimitError&
        ) {
            queue_error(
                connection,
                431,
                "Request Header Fields Too Large"
            );
        } catch (
            const std::length_error&
        ) {
            queue_error(
                connection,
                413,
                "Payload Too Large"
            );
        } catch (
            const std::invalid_argument&
        ) {
            queue_error(
                connection,
                400,
                "Bad Request"
            );
        } catch (...) {
            queue_error(
                connection,
                500,
                "Internal Server Error"
            );
        }
    }

    void prepare_response(
        ConnectionState& connection
    ) {
        if (
            !connection.output.empty()
        ) {
            return;
        }

        const auto expected =
            request_size(
                connection.input,
                options
            );

        if (!expected) {
            return;
        }

        auto raw =
            connection.input.substr(
                0,
                *expected
            );

        connection.input.erase(
            0,
            *expected
        );

        auto request =
            wire::parse_request(
                raw
            );

        auto response =
            complete_inline(
                router.dispatch(
                    request
                )
            );

        ++connection.requests_served;

        const auto keep_alive =
            keep_alive_for(
                raw,
                request,
                options,
                connection.requests_served
            );

        connection.output =
            wire::serialize_response(
                response,
                request.method() ==
                    Method::head,
                keep_alive
                    ? ConnectionDirective::
                        keep_alive
                    : ConnectionDirective::
                        close
            );

        connection.output_offset = 0;
        connection.close_after_write =
            !keep_alive;
        connection.phase_started =
            Clock::now();
    }

    void write_ready(
        ConnectionState& connection
    ) noexcept {
        try {
            while (
                connection.output_offset <
                connection.output.size()
            ) {
                const auto remaining =
                    connection.output.size() -
                    connection.output_offset;

#ifdef _WIN32
                const auto chunk =
                    std::min<std::size_t>(
                        remaining,
                        static_cast<std::size_t>(
                            std::numeric_limits<int>::
                                max()
                        )
                    );

                const auto written =
                    ::send(
                        connection.socket,
                        connection.output.data() +
                            connection.output_offset,
                        static_cast<int>(
                            chunk
                        ),
                        0
                    );
#else
                int flags = 0;

#ifdef MSG_NOSIGNAL
                flags = MSG_NOSIGNAL;
#endif

                const auto written =
                    ::send(
                        connection.socket,
                        connection.output.data() +
                            connection.output_offset,
                        remaining,
                        flags
                    );
#endif

                if (written > 0) {
                    connection.output_offset +=
                        static_cast<std::size_t>(
                            written
                        );

                    connection.last_activity =
                        Clock::now();

                    continue;
                }

                if (written == 0) {
                    close_connection(
                        connection
                    );
                    return;
                }

                const auto error =
                    socket_error();

                if (
                    interrupted(error)
                ) {
                    continue;
                }

                if (
                    would_block(error)
                ) {
                    return;
                }

                close_connection(
                    connection
                );
                return;
            }

            const auto close =
                connection.close_after_write;

            connection.output.clear();
            connection.output_offset = 0;
            connection.close_after_write =
                false;
            connection.phase_started =
                Clock::now();

            if (close) {
                close_connection(
                    connection
                );
                return;
            }

            if (
                !connection.input.empty()
            ) {
                prepare_response(
                    connection
                );
            }
        } catch (...) {
            close_connection(
                connection
            );
        }
    }

    void queue_error(
        ConnectionState& connection,
        int status,
        std::string body
    ) noexcept {
        try {
            connection.output =
                wire::serialize_response(
                    error_response(
                        status,
                        std::move(body)
                    ),
                    false,
                    ConnectionDirective::
                        close
                );

            connection.output_offset = 0;
            connection.close_after_write =
                true;
            connection.phase_started =
                Clock::now();
            connection.input.clear();
        } catch (...) {
            close_connection(
                connection
            );
        }
    }

    void expire_connections(
        Clock::time_point now
    ) noexcept {
        for (
            auto& connection :
            connections
        ) {
            if (connection.closing) {
                continue;
            }

            if (
                !connection.output.empty()
            ) {
                if (
                    now -
                        connection.phase_started >=
                    options.write_timeout
                ) {
                    close_connection(
                        connection
                    );
                }

                continue;
            }

            if (
                connection.input.empty() &&
                connection.requests_served > 0
            ) {
                if (
                    now -
                        connection.last_activity >=
                    options.idle_timeout
                ) {
                    close_connection(
                        connection
                    );
                }

                continue;
            }

            if (
                now -
                    connection.phase_started >=
                options.read_timeout
            ) {
                close_connection(
                    connection
                );
            }
        }
    }

    void remove_closed() {
        connections.erase(
            std::remove_if(
                connections.begin(),
                connections.end(),
                [](const auto& connection) {
                    return
                        connection.closing ||
                        connection.socket ==
                            invalid_socket;
                }
            ),
            connections.end()
        );
    }

    void close_all() noexcept {
        for (
            auto& connection :
            connections
        ) {
            close_connection(
                connection
            );
        }

        connections.clear();

        const auto socket =
            listener.exchange(
                invalid_socket
            );

        close_socket(socket);
    }

    void validate_options() const {
        if (
            options.max_request_bytes == 0
        ) {
            throw std::invalid_argument(
                "HTTP max_request_bytes must be greater than zero"
            );
        }

        if (
            options.max_header_bytes == 0 ||
            options.max_header_bytes >
                options.max_request_bytes
        ) {
            throw std::invalid_argument(
                "HTTP max_header_bytes must be between one and max_request_bytes"
            );
        }

        if (
            options.max_connections == 0
        ) {
            throw std::invalid_argument(
                "HTTP max_connections must be greater than zero"
            );
        }

        if (
            options.max_requests_per_connection ==
            0
        ) {
            throw std::invalid_argument(
                "HTTP max_requests_per_connection must be greater than zero"
            );
        }

        if (
            options.read_timeout.count() <= 0 ||
            options.write_timeout.count() <= 0 ||
            options.idle_timeout.count() <= 0 ||
            options.shutdown_timeout.count() <= 0
        ) {
            throw std::invalid_argument(
                "HTTP runtime timeouts must be greater than zero"
            );
        }
    }

    SocketRuntime socket_runtime;
    routing::Router& router;
    RuntimeOptions options;
    std::atomic_bool running{false};
    std::atomic<NativeSocket> listener{
        invalid_socket
    };
    std::atomic<std::uint16_t> bound{0};
    std::vector<ConnectionState> connections;
};

Server::Server(
    routing::Router& router,
    RuntimeOptions options
)
    : impl_(
        std::make_unique<Impl>(
            router,
            std::move(options)
        )
      ) {}

Server::~Server() = default;

void Server::listen(
    std::string host,
    std::uint16_t port
) {
    impl_->listen(
        std::move(host),
        port
    );
}

void Server::stop() noexcept {
    impl_->stop();
}

bool Server::running()
    const noexcept {
    return impl_->running.load();
}

std::uint16_t Server::bound_port()
    const noexcept {
    return impl_->bound.load();
}

const RuntimeOptions&
Server::options() const noexcept {
    return impl_->options;
}

} // namespace gungnir::http::detail
