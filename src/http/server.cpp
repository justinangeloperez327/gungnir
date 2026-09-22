#include <gungnir/http/server.hpp>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <condition_variable>
#include <coroutine>
#include <cstdint>
#include <deque>
#include <limits>
#include <mutex>
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
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace gungnir::http::detail {

namespace {

constexpr std::size_t max_request_bytes = 1024U * 1024U;
constexpr int listen_backlog = 256;

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

std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }

    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }

    return value;
}

std::size_t request_content_length(std::string_view headers) {
    std::size_t cursor = 0;

    while (cursor < headers.size()) {
        const auto line_end = headers.find("\r\n", cursor);
        const auto end = line_end == std::string_view::npos
            ? headers.size()
            : line_end;

        const auto line = headers.substr(cursor, end - cursor);
        const auto colon = line.find(':');

        if (colon != std::string_view::npos) {
            auto name = trim(line.substr(0, colon));
            const auto value = trim(line.substr(colon + 1));

            std::string lower{name};
            for (auto& character : lower) {
                if (character >= 'A' && character <= 'Z') {
                    character = static_cast<char>(character - 'A' + 'a');
                }
            }

            if (lower == "content-length") {
                std::size_t parsed = 0;
                const auto result = std::from_chars(
                    value.data(),
                    value.data() + value.size(),
                    parsed
                );

                if (
                    result.ec != std::errc{} ||
                    result.ptr != value.data() + value.size()
                ) {
                    throw std::invalid_argument("Invalid Content-Length");
                }

                return parsed;
            }
        }

        if (line_end == std::string_view::npos) {
            break;
        }
        cursor = line_end + 2;
    }

    return 0;
}

NativeSocket make_listener(
    const std::string& host,
    std::uint16_t port
) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = host.empty() ? AI_PASSIVE : 0;

    addrinfo* addresses = nullptr;
    const auto service = std::to_string(port);

    const auto status = getaddrinfo(
        host.empty() ? nullptr : host.c_str(),
        service.c_str(),
        &hints,
        &addresses
    );

    if (status != 0 || !addresses) {
        throw std::runtime_error("Unable to resolve HTTP listen address");
    }

    NativeSocket listener = invalid_socket;

    for (auto* current = addresses; current; current = current->ai_next) {
        const auto candidate = ::socket(
            current->ai_family,
            current->ai_socktype,
            current->ai_protocol
        );

        if (candidate == invalid_socket) {
            continue;
        }

        int enabled = 1;
#ifdef _WIN32
        setsockopt(
            candidate,
            SOL_SOCKET,
            SO_REUSEADDR,
            reinterpret_cast<const char*>(&enabled),
            static_cast<int>(sizeof(enabled))
        );
#else
        setsockopt(
            candidate,
            SOL_SOCKET,
            SO_REUSEADDR,
            &enabled,
            static_cast<socklen_t>(sizeof(enabled))
        );
#endif

        if (
            ::bind(
                candidate,
                current->ai_addr,
#ifdef _WIN32
                static_cast<int>(current->ai_addrlen)
#else
                current->ai_addrlen
#endif
            ) == 0 &&
            ::listen(candidate, listen_backlog) == 0
        ) {
            listener = candidate;
            break;
        }

        close_socket(candidate);
    }

    freeaddrinfo(addresses);

    if (listener == invalid_socket) {
        throw std::runtime_error("Unable to bind HTTP listener");
    }

    return listener;
}

std::string read_request(NativeSocket client) {
    std::string message;
    message.reserve(8192);

    std::size_t expected_size = std::numeric_limits<std::size_t>::max();
    char buffer[8192];

    while (message.size() < max_request_bytes) {
#ifdef _WIN32
        const auto received = ::recv(
            client,
            buffer,
            static_cast<int>(sizeof(buffer)),
            0
        );
#else
        const auto received = ::recv(client, buffer, sizeof(buffer), 0);
#endif

        if (received <= 0) {
            break;
        }

        message.append(buffer, static_cast<std::size_t>(received));

        const auto header_end = message.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            const auto body_size = request_content_length(
                std::string_view{message}.substr(0, header_end)
            );

            if (
                body_size >
                max_request_bytes - std::min(max_request_bytes, header_end + 4)
            ) {
                throw std::length_error("HTTP request is too large");
            }

            expected_size = header_end + 4 + body_size;
            if (message.size() >= expected_size) {
                message.resize(expected_size);
                return message;
            }
        }
    }

    if (message.size() >= max_request_bytes) {
        throw std::length_error("HTTP request is too large");
    }

    if (
        expected_size != std::numeric_limits<std::size_t>::max() &&
        message.size() < expected_size
    ) {
        throw std::invalid_argument("Incomplete HTTP request body");
    }

    return message;
}

void send_all(NativeSocket client, std::string_view message) {
    std::size_t sent = 0;

    while (sent < message.size()) {
        const auto remaining = message.size() - sent;
        const auto chunk = std::min<std::size_t>(
            remaining,
            static_cast<std::size_t>(
                std::numeric_limits<int>::max()
            )
        );

#ifdef _WIN32
        const auto written = ::send(
            client,
            message.data() + sent,
            static_cast<int>(chunk),
            0
        );
#else
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags = MSG_NOSIGNAL;
#endif
        const auto written = ::send(
            client,
            message.data() + sent,
            chunk,
            flags
        );
#endif

        if (written <= 0) {
            return;
        }

        sent += static_cast<std::size_t>(written);
    }
}

template <typename T>
T complete_inline(Task<T> task) {
    auto awaiter = task.operator co_await();

    if (!awaiter.await_ready()) {
        awaiter.await_suspend(std::noop_coroutine());
    }

    return awaiter.await_resume();
}

Response error_response(int status, std::string body) {
    return Response::text(std::move(body), status);
}

} // namespace

class Server::Impl {
public:
    explicit Impl(routing::Router& value, RuntimeOptions value_options)
        : router(value), options(std::move(value_options)) {}

    ~Impl() {
        stop();
        join_workers();
    }

    void listen(std::string host, std::uint16_t port) {
        bool expected = false;
        if (!running.compare_exchange_strong(expected, true)) {
            throw std::logic_error("Gungnir HTTP server is already running");
        }

        try {
            const auto socket = make_listener(host, port);
            listener.store(socket);
            start_workers();

            while (running.load()) {
                const auto active = listener.load();
                if (active == invalid_socket) {
                    break;
                }

                const auto client = ::accept(active, nullptr, nullptr);
                if (client == invalid_socket) {
                    if (!running.load()) {
                        break;
                    }
                    continue;
                }

                {
                    std::lock_guard lock{queue_mutex};
                    clients.push_back(client);
                }
                queue_ready.notify_one();
            }
        } catch (...) {
            stop();
            join_workers();
            throw;
        }

        stop();
        join_workers();
    }

    void stop() noexcept {
        if (!running.exchange(false)) {
            return;
        }

        const auto socket = listener.exchange(invalid_socket);
        close_socket(socket);
        queue_ready.notify_all();
    }

    void start_workers() {
        const auto hardware = std::thread::hardware_concurrency();
        const auto count = std::clamp<unsigned>(
            hardware == 0 ? 4U : hardware,
            2U,
            32U
        );

        workers.reserve(count);
        for (unsigned index = 0; index < count; ++index) {
            workers.emplace_back([this] { worker_loop(); });
        }
    }

    void join_workers() noexcept {
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers.clear();

        std::deque<NativeSocket> remaining;
        {
            std::lock_guard lock{queue_mutex};
            remaining.swap(clients);
        }

        for (const auto client : remaining) {
            close_socket(client);
        }
    }

    void worker_loop() noexcept {
        while (true) {
            NativeSocket client = invalid_socket;

            {
                std::unique_lock lock{queue_mutex};
                queue_ready.wait(lock, [this] {
                    return !running.load() || !clients.empty();
                });

                if (clients.empty()) {
                    if (!running.load()) {
                        return;
                    }
                    continue;
                }

                client = clients.front();
                clients.pop_front();
            }

            handle_client(client);
            close_socket(client);
        }
    }

    void handle_client(NativeSocket client) noexcept {
        try {
            const auto raw = read_request(client);
            auto request = wire::parse_request(raw);
            auto response = complete_inline(router.dispatch(request));
            const auto payload = wire::serialize_response(
                response,
                request.method() == Method::head
            );
            send_all(client, payload);
        } catch (const std::length_error&) {
            send_all(
                client,
                wire::serialize_response(
                    error_response(413, "Payload Too Large")
                )
            );
        } catch (const std::invalid_argument&) {
            send_all(
                client,
                wire::serialize_response(
                    error_response(400, "Bad Request")
                )
            );
        } catch (...) {
            send_all(
                client,
                wire::serialize_response(
                    error_response(500, "Internal Server Error")
                )
            );
        }
    }

    SocketRuntime socket_runtime;
    routing::Router& router;
    RuntimeOptions options;
    std::atomic_bool running{false};
    std::atomic<NativeSocket> listener{invalid_socket};
    std::mutex queue_mutex;
    std::condition_variable queue_ready;
    std::deque<NativeSocket> clients;
    std::vector<std::thread> workers;
};

Server::Server(routing::Router& router, RuntimeOptions options)
    : impl_(std::make_unique<Impl>(router, std::move(options))) {}

Server::~Server() = default;

void Server::listen(std::string host, std::uint16_t port) {
    impl_->listen(std::move(host), port);
}

void Server::stop() noexcept {
    impl_->stop();
}

bool Server::running() const noexcept {
    return impl_->running.load();
}

const RuntimeOptions& Server::options() const noexcept {
    return impl_->options;
}

} // namespace gungnir::http::detail
