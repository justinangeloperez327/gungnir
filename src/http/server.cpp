#include <gungnir/http/server.hpp>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include <gungnir/core/timer.hpp>
#include <gungnir/http/message.hpp>
#include <gungnir/observability/metrics.hpp>
#include <gungnir/observability/trace.hpp>
#include <gungnir/view/runtime.hpp>
#include <gungnir/routing/router.hpp>

#ifdef GUNGNIR_WITH_TLS
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace gungnir::http::detail {

namespace {

constexpr int listen_backlog = 256;
constexpr int reactor_poll_timeout_ms = 100;

#ifdef GUNGNIR_WITH_TLS

using SslContext =
    std::unique_ptr<
        SSL_CTX,
        decltype(&SSL_CTX_free)
    >;

using SslConnection =
    std::unique_ptr<
        SSL,
        decltype(&SSL_free)
    >;

[[nodiscard]]
std::string openssl_error(
    std::string_view prefix
) {
    std::ostringstream message;
    message << prefix;

    bool found = false;

    while (true) {
        const auto code =
            ERR_get_error();

        if (code == 0) {
            break;
        }

        char buffer[
            256
        ]{};

        ERR_error_string_n(
            code,
            buffer,
            sizeof(buffer)
        );

        message <<
            (found ? "; " : ": ") <<
            buffer;

        found = true;
    }

    return message.str();
}

[[nodiscard]]
bool alpn_contains(
    const unsigned char* protocols,
    unsigned int protocols_length,
    std::string_view expected
) noexcept {
    unsigned int cursor = 0;

    while (cursor < protocols_length) {
        const auto length =
            static_cast<unsigned int>(
                protocols[cursor]
            );

        ++cursor;

        if (
            length == 0 ||
            cursor + length >
                protocols_length
        ) {
            return false;
        }

        const std::string_view value{
            reinterpret_cast<
                const char*
            >(
                protocols + cursor
            ),
            length
        };

        if (value == expected) {
            return true;
        }

        cursor += length;
    }

    return false;
}

int select_alpn(
    SSL*,
    const unsigned char** output,
    unsigned char* output_length,
    const unsigned char* input,
    unsigned int input_length,
    void* context
) noexcept {
    const auto* configured =
        static_cast<
            const std::vector<
                std::string
            >*
        >(context);

    if (configured == nullptr) {
        return
            SSL_TLSEXT_ERR_NOACK;
    }

    for (
        const auto& protocol :
        *configured
    ) {
        if (
            protocol.size() >
            255 ||
            !alpn_contains(
                input,
                input_length,
                protocol
            )
        ) {
            continue;
        }

        *output =
            reinterpret_cast<
                const unsigned char*
            >(
                protocol.data()
            );

        *output_length =
            static_cast<unsigned char>(
                protocol.size()
            );

        return
            SSL_TLSEXT_ERR_OK;
    }

    return
        SSL_TLSEXT_ERR_NOACK;
}

[[nodiscard]]
SslContext make_tls_context(
    const TlsOptions& options
) {
    ERR_clear_error();

    SslContext context{
        SSL_CTX_new(
            TLS_server_method()
        ),
        &SSL_CTX_free
    };

    if (!context) {
        throw std::runtime_error(
            openssl_error(
                "Unable to create TLS server context"
            )
        );
    }

    if (
        SSL_CTX_set_min_proto_version(
            context.get(),
            TLS1_2_VERSION
        ) != 1
    ) {
        throw std::runtime_error(
            openssl_error(
                "Unable to require TLS 1.2 or newer"
            )
        );
    }

    SSL_CTX_set_options(
        context.get(),
        SSL_OP_NO_COMPRESSION
    );

    if (
        !options
            .private_key_password
            .empty()
    ) {
        SSL_CTX_set_default_passwd_cb_userdata(
            context.get(),
            const_cast<char*>(
                options
                    .private_key_password
                    .c_str()
            )
        );

        SSL_CTX_set_default_passwd_cb(
            context.get(),
            [](
                char* buffer,
                int size,
                int,
                void* userdata
            ) -> int {
                if (
                    buffer == nullptr ||
                    size <= 0 ||
                    userdata == nullptr
                ) {
                    return 0;
                }

                const auto* password =
                    static_cast<
                        const char*
                    >(userdata);

                const auto length =
                    std::min<std::size_t>(
                        std::strlen(
                            password
                        ),
                        static_cast<
                            std::size_t
                        >(size - 1)
                    );

                std::memcpy(
                    buffer,
                    password,
                    length
                );

                buffer[length] = '\0';

                return
                    static_cast<int>(
                        length
                    );
            }
        );
    }

    if (
        SSL_CTX_use_certificate_chain_file(
            context.get(),
            options
                .certificate_chain
                .c_str()
        ) != 1
    ) {
        throw std::runtime_error(
            openssl_error(
                "Unable to load TLS certificate chain"
            )
        );
    }

    if (
        SSL_CTX_use_PrivateKey_file(
            context.get(),
            options
                .private_key
                .c_str(),
            SSL_FILETYPE_PEM
        ) != 1
    ) {
        throw std::runtime_error(
            openssl_error(
                "Unable to load TLS private key"
            )
        );
    }

    if (
        SSL_CTX_check_private_key(
            context.get()
        ) != 1
    ) {
        throw std::runtime_error(
            openssl_error(
                "TLS private key does not match certificate"
            )
        );
    }

    SSL_CTX_set_default_passwd_cb(
        context.get(),
        nullptr
    );

    SSL_CTX_set_default_passwd_cb_userdata(
        context.get(),
        nullptr
    );

    if (
        !options
            .client_ca
            .empty()
    ) {
        if (
            SSL_CTX_load_verify_locations(
                context.get(),
                options
                    .client_ca
                    .c_str(),
                nullptr
            ) != 1
        ) {
            throw std::runtime_error(
                openssl_error(
                    "Unable to load TLS client CA bundle"
                )
            );
        }
    }

    if (
        options
            .require_client_certificate
    ) {
        SSL_CTX_set_verify(
            context.get(),
            SSL_VERIFY_PEER |
                SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
            nullptr
        );
    } else {
        SSL_CTX_set_verify(
            context.get(),
            SSL_VERIFY_NONE,
            nullptr
        );
    }

    SSL_CTX_set_alpn_select_cb(
        context.get(),
        &select_alpn,
        const_cast<
            std::vector<
                std::string
            >*
        >(
            &options.alpn_protocols
        )
    );

    return context;
}

#endif

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

class WakeState {
public:
    WakeState() {
        receiver_ = ::socket(
            AF_INET,
            SOCK_DGRAM,
            IPPROTO_UDP
        );

        if (
            receiver_ ==
            invalid_socket
        ) {
            throw std::runtime_error(
                "Unable to create HTTP reactor wake receiver"
            );
        }

        sender_ = ::socket(
            AF_INET,
            SOCK_DGRAM,
            IPPROTO_UDP
        );

        if (
            sender_ ==
            invalid_socket
        ) {
            close_socket(receiver_);
            receiver_ = invalid_socket;

            throw std::runtime_error(
                "Unable to create HTTP reactor wake sender"
            );
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr =
            htonl(INADDR_LOOPBACK);
        address.sin_port = 0;

        if (
            ::bind(
                receiver_,
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
            fail(
                "Unable to bind HTTP reactor wake receiver"
            );
        }

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
                receiver_,
                reinterpret_cast<
                    sockaddr*
                >(&address),
                &length
            ) != 0
        ) {
            fail(
                "Unable to inspect HTTP reactor wake receiver"
            );
        }

        if (
            ::connect(
                sender_,
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
            fail(
                "Unable to connect HTTP reactor wake sender"
            );
        }

        if (
            !set_non_blocking(receiver_) ||
            !set_non_blocking(sender_)
        ) {
            fail(
                "Unable to configure HTTP reactor wake sockets"
            );
        }

        active_ = true;
    }

    ~WakeState() {
        shutdown();
    }

    WakeState(const WakeState&) = delete;
    WakeState& operator=(const WakeState&) = delete;

    [[nodiscard]]
    NativeSocket reader()
        const noexcept {
        return receiver_;
    }

    void notify() noexcept {
        std::lock_guard lock{
            mutex_
        };

        if (!active_) {
            return;
        }

        constexpr char value = 1;

#ifdef _WIN32
        static_cast<void>(
            ::send(
                sender_,
                &value,
                1,
                0
            )
        );
#else
        static_cast<void>(
            ::send(
                sender_,
                &value,
                1,
                0
            )
        );
#endif
    }

    void drain() noexcept {
        char buffer[64];

        while (true) {
#ifdef _WIN32
            const auto received =
                ::recv(
                    receiver_,
                    buffer,
                    static_cast<int>(
                        sizeof(buffer)
                    ),
                    0
                );
#else
            const auto received =
                ::recv(
                    receiver_,
                    buffer,
                    sizeof(buffer),
                    0
                );
#endif

            if (received > 0) {
                continue;
            }

            if (received == 0) {
                return;
            }

            const auto error =
                socket_error();

            if (interrupted(error)) {
                continue;
            }

            return;
        }
    }

    void shutdown() noexcept {
        std::lock_guard lock{
            mutex_
        };

        if (!active_) {
            return;
        }

        active_ = false;

        close_socket(receiver_);
        close_socket(sender_);

        receiver_ = invalid_socket;
        sender_ = invalid_socket;
    }

private:
    [[noreturn]]
    void fail(
        const char* message
    ) {
        close_socket(receiver_);
        close_socket(sender_);

        receiver_ = invalid_socket;
        sender_ = invalid_socket;

        throw std::runtime_error(
            message
        );
    }

    std::mutex mutex_;
    NativeSocket receiver_{
        invalid_socket
    };
    NativeSocket sender_{
        invalid_socket
    };
    bool active_{false};
};

void wake_timer_pump(
    void* context
) noexcept {
    if (context == nullptr) {
        return;
    }

    static_cast<WakeState*>(
        context
    )->notify();
}

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

Response error_response(
    int status,
    std::string body
) {
    return Response::text(
        std::move(body),
        status
    );
}

struct PendingDispatch {
    PendingDispatch(
        Request value,
        CancellationSource source,
        std::shared_ptr<view::Engine>
            value_view_engine
    )
        : request(
            std::move(value)
          ),
          cancellation(
            std::move(source)
          ),
          view_engine(
            std::move(
                value_view_engine
            )
          ),
          span(
            observability::
                global_tracer()
                ->start_span(
                    "http.server.request",
                    {
                        {
                            "http.request.method",
                            std::string{
                                to_string(
                                    request.method()
                                )
                            }
                        },
                        {
                            "url.path",
                            std::string{
                                request.path()
                            }
                        }
                    }
                )
          ) {}

    void cancel() noexcept {
        cancellation.cancel();
    }

    Request request;
    CancellationSource cancellation;
    std::shared_ptr<view::Engine>
        view_engine;
    observability::Span span;
    Clock::time_point metric_started{
        Clock::now()
    };
    std::optional<Response> response;
    std::exception_ptr exception;
    std::atomic_bool ready{false};
    bool keep_alive{false};
    bool omit_body{false};
};

class DispatchTracker {
public:
    void begin() {
        std::lock_guard lock{
            mutex_
        };

        ++active_;
    }

    void finish() noexcept {
        {
            std::lock_guard lock{
                mutex_
            };

            if (active_ > 0) {
                --active_;
            }
        }

        ready_.notify_all();
    }

    void wait() {
        std::unique_lock lock{
            mutex_
        };

        ready_.wait(
            lock,
            [this] {
                return active_ == 0;
            }
        );
    }

    [[nodiscard]]
    std::size_t active()
        const noexcept {
        std::lock_guard lock{
            mutex_
        };

        return active_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::size_t active_{0};
};

class DetachedTask {
public:
    struct promise_type {
        [[nodiscard]]
        DetachedTask
        get_return_object()
            const noexcept {
            return {};
        }

        [[nodiscard]]
        std::suspend_never
        initial_suspend()
            const noexcept {
            return {};
        }

        [[nodiscard]]
        std::suspend_never
        final_suspend()
            const noexcept {
            return {};
        }

        void return_void()
            const noexcept {}

        void unhandled_exception()
            const noexcept {
            std::terminate();
        }
    };
};

DetachedTask settle_dispatch(
    Task<Response> task,
    std::shared_ptr<PendingDispatch> pending,
    std::shared_ptr<WakeState> wakeup,
    std::shared_ptr<DispatchTracker> dispatches
) {
    dispatches->begin();

    auto scope =
        pending->span.valid()
            ? pending->span.scope()
            : observability::Scope{};

    auto view_scope =
        view::runtime::activate(
            pending->view_engine
        );

    int response_status = 500;
    std::string outcome{"error"};

    try {
        pending->response.emplace(
            co_await task
        );

        response_status =
            pending->response->status();

        pending->span.attribute(
            "http.response.status_code",
            std::to_string(
                response_status
            )
        );

        if (response_status >= 500) {
            pending->span.error(
                "HTTP server error response"
            );
        } else {
            pending->span.status(
                observability::
                    SpanStatus::ok
            );

            outcome = "ok";
        }
    } catch (
        const std::exception& error
    ) {
        pending->span.error(
            error.what()
        );

        pending->exception =
            std::current_exception();
    } catch (...) {
        pending->span.error(
            "Unknown HTTP dispatch exception"
        );

        pending->exception =
            std::current_exception();
    }

    const auto elapsed =
        std::chrono::duration<
            double,
            std::milli
        >(
            Clock::now() -
            pending->metric_started
        ).count();

    auto metric_attributes =
        observability::Attributes{
            {
                "http.request.method",
                std::string{
                    to_string(
                        pending->request.method()
                    )
                }
            },
            {
                "url.path",
                std::string{
                    pending->request.path()
                }
            },
            {
                "http.response.status_code",
                std::to_string(
                    response_status
                )
            },
            {
                "outcome",
                outcome
            }
        };

    auto meter =
        observability::
            global_meter();

    meter->histogram(
        "http.server.request.duration"
    ).record(
        elapsed,
        metric_attributes
    );

    meter->counter(
        "http.server.request.count"
    ).add(
        1.0,
        std::move(
            metric_attributes
        )
    );

    pending->span.end();

    pending->ready.store(
        true,
        std::memory_order_release
    );

    wakeup->notify();
    dispatches->finish();
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
    std::shared_ptr<PendingDispatch> pending;

#ifdef GUNGNIR_WITH_TLS
    SslConnection tls{
        nullptr,
        &SSL_free
    };
    bool tls_handshake_complete{false};
    bool tls_shutdown_pending{false};
    bool tls_want_read{false};
    bool tls_want_write{false};
#endif

    bool close_after_write{false};
    bool closing{false};
};

void close_connection(
    ConnectionState& connection
) noexcept {
    if (connection.pending) {
        connection.pending->cancel();
    }

#ifdef GUNGNIR_WITH_TLS
    connection.tls.reset();
#endif

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
        dispatches->wait();
        wakeup->shutdown();
    }

    void listen(
        std::string host,
        std::uint16_t port,
        CancellationToken cancellation
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

        const auto cancellation_registration =
            cancellation.on_cancel(
                [this] {
                    stop();
                }
            );

        static_cast<void>(
            cancellation_registration
        );

        if (!running.load()) {
            return;
        }

        try {
            prepare_tls_context();

            const auto socket =
                make_listener(
                    host,
                    port
                );

            listener.store(socket);
            bound.store(
                socket_port(socket)
            );

            timer_pump_token =
                ::gungnir::detail::
                    attach_timer_pump(
                        wakeup.get(),
                        &wake_timer_pump
                    );

            reactor_loop();

            ::gungnir::detail::
                detach_timer_pump(
                    timer_pump_token
                );

            timer_pump_token = 0;
        } catch (...) {
            ::gungnir::detail::
                detach_timer_pump(
                    timer_pump_token
                );

            timer_pump_token = 0;
            running.store(false);

            const auto socket =
                listener.exchange(
                    invalid_socket
                );

            close_socket(socket);
            bound.store(0);
            close_all();
            dispatches->wait();
            throw;
        }

        const auto socket =
            listener.exchange(
                invalid_socket
            );

        close_socket(socket);
        close_all();
        dispatches->wait();
        bound.store(0);
        running.store(false);
    }

    void stop() noexcept {
        running.store(false);
        wakeup->notify();
    }

    void configure(
        RuntimeOptions value
    ) {
        if (running.load()) {
            throw std::logic_error(
                "HTTP runtime options cannot change while the server is running"
            );
        }

        const auto previous =
            options;

        options =
            std::move(value);

        try {
            validate_options();
        } catch (...) {
            options = previous;
            throw;
        }
    }

    void set_view_engine(
        std::shared_ptr<view::Engine>
            value
    ) {
        if (running.load()) {
            throw std::logic_error(
                "HTTP view engine cannot change while the server is running"
            );
        }

        view_engine =
            std::move(value);
    }

    void prepare_tls_context() {
        if (!options.tls) {
#ifdef GUNGNIR_WITH_TLS
            tls_context.reset();
#endif
            return;
        }

#ifndef GUNGNIR_WITH_TLS
        throw std::logic_error(
            "Gungnir was built without TLS transport support"
        );
#else
        tls_context =
            make_tls_context(
                *options.tls
            );
#endif
    }

#ifdef GUNGNIR_WITH_TLS
    [[nodiscard]]
    bool attach_tls(
        ConnectionState& connection
    ) noexcept {
        if (!tls_context) {
            return true;
        }

#ifdef _WIN32
        if (
            static_cast<std::uint64_t>(
                connection.socket
            ) >
            static_cast<std::uint64_t>(
                std::numeric_limits<int>::
                    max()
            )
        ) {
            return false;
        }
#endif

        ERR_clear_error();

        SslConnection session{
            SSL_new(
                tls_context.get()
            ),
            &SSL_free
        };

        if (!session) {
            return false;
        }

        if (
            SSL_set_fd(
                session.get(),
                static_cast<int>(
                    connection.socket
                )
            ) != 1
        ) {
            return false;
        }

        SSL_set_accept_state(
            session.get()
        );

        SSL_set_mode(
            session.get(),
            SSL_MODE_ENABLE_PARTIAL_WRITE |
                SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER
        );

        connection.tls =
            std::move(session);

        connection.tls_handshake_complete =
            false;

        connection.tls_want_read =
            true;

        connection.tls_want_write =
            false;

        return true;
    }

    [[nodiscard]]
    bool tls_handshake_ready(
        ConnectionState& connection
    ) noexcept {
        if (
            !connection.tls ||
            connection
                .tls_handshake_complete
        ) {
            return true;
        }

        connection.tls_want_read =
            false;

        connection.tls_want_write =
            false;

        ERR_clear_error();

        const auto result =
            SSL_accept(
                connection.tls.get()
            );

        if (result == 1) {
            connection
                .tls_handshake_complete =
                true;

            connection.last_activity =
                Clock::now();

            observability::
                global_meter()
                ->counter(
                    "http.server.tls.handshake.count"
                )
                .add(
                    1.0,
                    {
                        {
                            "outcome",
                            "ok"
                        }
                    }
                );

            return true;
        }

        const auto error =
            SSL_get_error(
                connection.tls.get(),
                result
            );

        if (
            error ==
            SSL_ERROR_WANT_READ
        ) {
            connection.tls_want_read =
                true;

            return false;
        }

        if (
            error ==
            SSL_ERROR_WANT_WRITE
        ) {
            connection.tls_want_write =
                true;

            return false;
        }

        observability::
            global_meter()
            ->counter(
                "http.server.tls.handshake.count"
            )
            .add(
                1.0,
                {
                    {
                        "outcome",
                        "error"
                    }
                }
            );

        close_connection(
            connection
        );

        return false;
    }
#endif

    void graceful_close(
        ConnectionState& connection
    ) noexcept {
#ifdef GUNGNIR_WITH_TLS
        if (
            connection.tls &&
            connection
                .tls_handshake_complete
        ) {
            connection.tls_want_read =
                false;

            connection.tls_want_write =
                false;

            ERR_clear_error();

            const auto status =
                SSL_shutdown(
                    connection.tls.get()
                );

            if (status >= 0) {
                close_connection(
                    connection
                );

                return;
            }

            const auto error =
                SSL_get_error(
                    connection.tls.get(),
                    status
                );

            if (
                error ==
                SSL_ERROR_WANT_READ
            ) {
                connection
                    .tls_shutdown_pending =
                    true;

                connection.tls_want_read =
                    true;

                return;
            }

            if (
                error ==
                SSL_ERROR_WANT_WRITE
            ) {
                connection
                    .tls_shutdown_pending =
                    true;

                connection.tls_want_write =
                    true;

                return;
            }
        }
#endif

        close_connection(
            connection
        );
    }

#ifdef GUNGNIR_WITH_TLS
    void tls_shutdown_ready(
        ConnectionState& connection
    ) noexcept {
        if (
            !connection.tls ||
            !connection
                .tls_shutdown_pending
        ) {
            return;
        }

        connection.tls_want_read =
            false;

        connection.tls_want_write =
            false;

        ERR_clear_error();

        const auto status =
            SSL_shutdown(
                connection.tls.get()
            );

        if (status >= 0) {
            close_connection(
                connection
            );

            return;
        }

        const auto error =
            SSL_get_error(
                connection.tls.get(),
                status
            );

        if (
            error ==
            SSL_ERROR_WANT_READ
        ) {
            connection.tls_want_read =
                true;

            return;
        }

        if (
            error ==
            SSL_ERROR_WANT_WRITE
        ) {
            connection.tls_want_write =
                true;

            return;
        }

        close_connection(
            connection
        );
    }
#endif

    enum class IoState {
        progress,
        would_block,
        closed,
        error
    };

    struct IoResult {
        IoState state{
            IoState::error
        };
        std::size_t count{0};
    };

    [[nodiscard]]
    IoResult receive_bytes(
        ConnectionState& connection,
        char* buffer,
        std::size_t capacity
    ) noexcept {
#ifdef GUNGNIR_WITH_TLS
        if (connection.tls) {
            connection.tls_want_read =
                false;

            connection.tls_want_write =
                false;

            while (true) {
                std::size_t received = 0;

                ERR_clear_error();

                const auto status =
                    SSL_read_ex(
                        connection.tls.get(),
                        buffer,
                        capacity,
                        &received
                    );

                if (status == 1) {
                    return {
                        IoState::progress,
                        received
                    };
                }

                const auto error =
                    SSL_get_error(
                        connection.tls.get(),
                        status
                    );

                if (
                    error ==
                    SSL_ERROR_WANT_READ
                ) {
                    connection.tls_want_read =
                        true;

                    return {
                        IoState::would_block,
                        0
                    };
                }

                if (
                    error ==
                    SSL_ERROR_WANT_WRITE
                ) {
                    connection.tls_want_write =
                        true;

                    return {
                        IoState::would_block,
                        0
                    };
                }

                if (
                    error ==
                    SSL_ERROR_ZERO_RETURN
                ) {
                    return {
                        IoState::closed,
                        0
                    };
                }

                if (
                    error ==
                        SSL_ERROR_SYSCALL &&
                    interrupted(
                        socket_error()
                    )
                ) {
                    continue;
                }

                return {
                    IoState::error,
                    0
                };
            }
        }
#endif

        while (true) {
#ifdef _WIN32
            const auto chunk =
                std::min<std::size_t>(
                    capacity,
                    static_cast<std::size_t>(
                        std::numeric_limits<int>::
                            max()
                    )
                );

            const auto received =
                ::recv(
                    connection.socket,
                    buffer,
                    static_cast<int>(
                        chunk
                    ),
                    0
                );
#else
            const auto received =
                ::recv(
                    connection.socket,
                    buffer,
                    capacity,
                    0
                );
#endif

            if (received > 0) {
                return {
                    IoState::progress,
                    static_cast<
                        std::size_t
                    >(received)
                };
            }

            if (received == 0) {
                return {
                    IoState::closed,
                    0
                };
            }

            const auto error =
                socket_error();

            if (interrupted(error)) {
                continue;
            }

            if (would_block(error)) {
                return {
                    IoState::would_block,
                    0
                };
            }

            return {
                IoState::error,
                0
            };
        }
    }

    [[nodiscard]]
    IoResult send_bytes(
        ConnectionState& connection,
        const char* data,
        std::size_t size
    ) noexcept {
#ifdef GUNGNIR_WITH_TLS
        if (connection.tls) {
            connection.tls_want_read =
                false;

            connection.tls_want_write =
                false;

            while (true) {
                std::size_t written = 0;

                ERR_clear_error();

                const auto status =
                    SSL_write_ex(
                        connection.tls.get(),
                        data,
                        size,
                        &written
                    );

                if (status == 1) {
                    return {
                        IoState::progress,
                        written
                    };
                }

                const auto error =
                    SSL_get_error(
                        connection.tls.get(),
                        status
                    );

                if (
                    error ==
                    SSL_ERROR_WANT_READ
                ) {
                    connection.tls_want_read =
                        true;

                    return {
                        IoState::would_block,
                        0
                    };
                }

                if (
                    error ==
                    SSL_ERROR_WANT_WRITE
                ) {
                    connection.tls_want_write =
                        true;

                    return {
                        IoState::would_block,
                        0
                    };
                }

                if (
                    error ==
                    SSL_ERROR_ZERO_RETURN
                ) {
                    return {
                        IoState::closed,
                        0
                    };
                }

                if (
                    error ==
                        SSL_ERROR_SYSCALL &&
                    interrupted(
                        socket_error()
                    )
                ) {
                    continue;
                }

                return {
                    IoState::error,
                    0
                };
            }
        }
#endif

        while (true) {
#ifdef _WIN32
            const auto chunk =
                std::min<std::size_t>(
                    size,
                    static_cast<std::size_t>(
                        std::numeric_limits<int>::
                            max()
                    )
                );

            const auto written =
                ::send(
                    connection.socket,
                    data,
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
                    data,
                    size,
                    flags
                );
#endif

            if (written > 0) {
                return {
                    IoState::progress,
                    static_cast<
                        std::size_t
                    >(written)
                };
            }

            if (written == 0) {
                return {
                    IoState::closed,
                    0
                };
            }

            const auto error =
                socket_error();

            if (interrupted(error)) {
                continue;
            }

            if (would_block(error)) {
                return {
                    IoState::would_block,
                    0
                };
            }

            return {
                IoState::error,
                0
            };
        }
    }

    void reactor_loop() {
        std::optional<
            Clock::time_point
        > drain_deadline;

        while (true) {
            ::gungnir::detail::
                dispatch_due_timers();

            complete_ready_handlers();

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
                    const auto has_in_flight_work =
                        connection.pending ||
                        !connection.output.empty();

                    if (!has_in_flight_work) {
                        close_connection(
                            connection
                        );
                        continue;
                    }

                    connection.input.clear();
                    connection.close_after_write =
                        true;
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
                for (
                    auto& connection :
                    connections
                ) {
                    if (connection.pending) {
                        connection.pending->cancel();
                    }
                }

                close_all();
                return;
            }

            expire_connections(now);
            remove_closed();

            std::vector<PollFd> descriptors;
            descriptors.reserve(
                connections.size() + 2
            );

            descriptors.push_back(
                PollFd{
                    wakeup->reader(),
                    poll_read_event,
                    0
                }
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

#ifdef GUNGNIR_WITH_TLS
                if (
                    connection.tls &&
                    !connection
                        .tls_handshake_complete
                ) {
                    if (
                        connection
                            .tls_want_write
                    ) {
                        events |=
                            poll_write_event;
                    }

                    if (
                        connection
                            .tls_want_read ||
                        events == 0
                    ) {
                        events |=
                            poll_read_event;
                    }
                } else if (
                    connection.tls &&
                    connection
                        .tls_shutdown_pending
                ) {
                    if (
                        connection
                            .tls_want_read
                    ) {
                        events |=
                            poll_read_event;
                    }

                    if (
                        connection
                            .tls_want_write ||
                        events == 0
                    ) {
                        events |=
                            poll_write_event;
                    }
                } else if (
                    connection.tls &&
                    connection
                        .tls_handshake_complete &&
                    (
                        connection
                            .tls_want_read ||
                        connection
                            .tls_want_write
                    )
                ) {
                    if (
                        connection
                            .tls_want_read
                    ) {
                        events |=
                            poll_read_event;
                    }

                    if (
                        connection
                            .tls_want_write
                    ) {
                        events |=
                            poll_write_event;
                    }
                } else
#endif
                if (connection.pending) {
                    events =
                        poll_read_event;
                } else if (
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

            const auto timer_timeout =
                ::gungnir::detail::
                    timer_poll_timeout(
                        std::chrono::milliseconds{
                            reactor_poll_timeout_ms
                        }
                    );

            const auto status =
                poll_sockets(
                    descriptors.data(),
                    descriptors.size(),
                    static_cast<int>(
                        timer_timeout.count()
                    )
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

            if (
                (
                    descriptors.front()
                        .revents &
                    poll_read_event
                ) != 0
            ) {
                wakeup->drain();

                ::gungnir::detail::
                    dispatch_due_timers();

                complete_ready_handlers();
            }

            std::size_t offset = 1;

            if (poll_listener) {
                const auto events =
                    descriptors[offset]
                        .revents;

                if (
                    (
                        events &
                        poll_read_event
                    ) != 0
                ) {
                    accept_ready();
                }

                ++offset;
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

#ifdef GUNGNIR_WITH_TLS
                if (
                    connection.tls &&
                    connection
                        .tls_shutdown_pending &&
                    (
                        (
                            events &
                            poll_read_event
                        ) != 0 ||
                        (
                            events &
                            poll_write_event
                        ) != 0
                    )
                ) {
                    tls_shutdown_ready(
                        connection
                    );

                    continue;
                }

                if (
                    connection.tls &&
                    !connection
                        .tls_handshake_complete &&
                    (
                        (
                            events &
                            poll_read_event
                        ) != 0 ||
                        (
                            events &
                            poll_write_event
                        ) != 0
                    )
                ) {
                    if (
                        !tls_handshake_ready(
                            connection
                        )
                    ) {
                        continue;
                    }

                    read_ready(
                        connection
                    );

                    continue;
                }
#endif

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

#ifdef GUNGNIR_WITH_TLS
                if (
                    !connection.closing &&
                    connection.tls &&
                    connection
                        .tls_handshake_complete &&
                    connection
                        .tls_want_write &&
                    (
                        events &
                        poll_write_event
                    ) != 0 &&
                    connection.output.empty()
                ) {
                    read_ready(
                        connection
                    );
                }

                if (
                    !connection.closing &&
                    connection.tls &&
                    connection
                        .tls_handshake_complete &&
                    connection
                        .tls_want_read &&
                    (
                        events &
                        poll_read_event
                    ) != 0 &&
                    !connection.output.empty()
                ) {
                    write_ready(
                        connection
                    );
                }
#endif
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

#ifdef GUNGNIR_WITH_TLS
            if (
                !attach_tls(
                    connection
                )
            ) {
                close_socket(client);
                continue;
            }
#endif

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

            publish_connection_gauge();
        }
    }

    void read_ready(
        ConnectionState& connection
    ) noexcept {
        try {
            char buffer[8192];

            while (true) {
                const auto received =
                    receive_bytes(
                        connection,
                        buffer,
                        sizeof(buffer)
                    );

                if (
                    received.state ==
                    IoState::progress
                ) {
                    const auto new_request =
                        connection.input.empty();

                    connection.input.append(
                        buffer,
                        received.count
                    );

                    const auto activity =
                        Clock::now();

                    if (new_request) {
                        connection.phase_started =
                            activity;
                    }

                    connection.last_activity =
                        activity;

                    if (
                        request_size(
                            connection.input,
                            options
                        )
                    ) {
                        break;
                    }

                    continue;
                }

                if (
                    received.state ==
                    IoState::would_block
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

    void complete_ready_handler(
        ConnectionState& connection
    ) noexcept {
        if (
            connection.closing ||
            !connection.pending ||
            !connection.pending->ready.load(
                std::memory_order_acquire
            )
        ) {
            return;
        }

        auto pending =
            std::move(
                connection.pending
            );

        try {
            const auto failed =
                pending->exception ||
                !pending->response;

            auto response =
                failed
                ? error_response(
                    500,
                    "Internal Server Error"
                  )
                : std::move(
                    *pending->response
                  );

            const auto keep_alive =
                !failed &&
                running.load() &&
                pending->keep_alive &&
                !connection.close_after_write;

            connection.output =
                wire::serialize_response(
                    response,
                    pending->omit_body,
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
        } catch (...) {
            queue_error(
                connection,
                500,
                "Internal Server Error"
            );
        }
    }

    void complete_ready_handlers()
        noexcept {
        for (
            auto& connection :
            connections
        ) {
            complete_ready_handler(
                connection
            );
        }
    }

    void prepare_response(
        ConnectionState& connection
    ) {
        if (
            !connection.output.empty() ||
            connection.pending
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

        CancellationSource cancellation;

        auto request =
            wire::parse_request(
                raw,
                cancellation.token(),
#ifdef GUNGNIR_WITH_TLS
                static_cast<bool>(
                    connection.tls
                )
#else
                false
#endif
            );

        ++connection.requests_served;

        auto pending =
            std::make_shared<
                PendingDispatch
            >(
                std::move(request),
                std::move(cancellation),
                view_engine
            );

        pending->keep_alive =
            running.load() &&
            keep_alive_for(
                raw,
                pending->request,
                options,
                connection.requests_served
            );

        pending->omit_body =
            pending->request.method() ==
            Method::head;

        connection.pending =
            pending;

        connection.phase_started =
            Clock::now();

        auto task =
            router.dispatch(
                pending->request
            );

        settle_dispatch(
            std::move(task),
            pending,
            wakeup,
            dispatches
        );

        complete_ready_handler(
            connection
        );
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

                const auto written =
                    send_bytes(
                        connection,
                        connection.output.data() +
                            connection.output_offset,
                        remaining
                    );

                if (
                    written.state ==
                    IoState::progress
                ) {
                    connection.output_offset +=
                        written.count;

                    connection.last_activity =
                        Clock::now();

                    continue;
                }

                if (
                    written.state ==
                    IoState::would_block
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
                graceful_close(
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

            if (connection.pending) {
                connection.pending->cancel();
            }

            connection.pending.reset();
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

            if (connection.pending) {
                if (
                    now -
                        connection.phase_started >=
                    options.request_timeout
                ) {
                    close_connection(
                        connection
                    );
                }

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

        publish_connection_gauge();
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

        publish_connection_gauge();

        const auto socket =
            listener.exchange(
                invalid_socket
            );

        close_socket(socket);
    }

    void publish_connection_gauge()
        noexcept {
        try {
            observability::
                global_meter()
                ->gauge(
                    "http.server.connection.active"
                )
                .set(
                    static_cast<double>(
                        connections.size()
                    )
                );
        } catch (...) {
        }
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
            options.request_timeout.count() <= 0 ||
            options.shutdown_timeout.count() <= 0
        ) {
            throw std::invalid_argument(
                "HTTP runtime timeouts must be greater than zero"
            );
        }

        if (options.tls) {
            const auto& tls =
                *options.tls;

            if (
                tls.certificate_chain.empty() ||
                tls.private_key.empty()
            ) {
                throw std::invalid_argument(
                    "TLS certificate_chain and private_key are required"
                );
            }

            if (
                tls.require_client_certificate &&
                tls.client_ca.empty()
            ) {
                throw std::invalid_argument(
                    "TLS client_ca is required when client certificates are mandatory"
                );
            }

            if (
                tls.alpn_protocols.empty()
            ) {
                throw std::invalid_argument(
                    "TLS ALPN protocol list must not be empty"
                );
            }

            std::unordered_set<
                std::string
            > protocols;

            for (
                const auto& protocol :
                tls.alpn_protocols
            ) {
                if (
                    protocol.empty() ||
                    protocol.size() > 255
                ) {
                    throw std::invalid_argument(
                        "TLS ALPN protocol names must contain 1-255 bytes"
                    );
                }

                if (
                    protocol !=
                    "http/1.1"
                ) {
                    throw std::invalid_argument(
                        "The core TLS server currently supports HTTP/1.1 ALPN only"
                    );
                }

                if (
                    !protocols
                        .insert(protocol)
                        .second
                ) {
                    throw std::invalid_argument(
                        "TLS ALPN protocol names must be unique"
                    );
                }
            }

#ifndef GUNGNIR_WITH_TLS
            throw std::logic_error(
                "TLS runtime configuration requires a GUNGNIR_WITH_TLS build"
            );
#endif
        }
    }

    SocketRuntime socket_runtime;
    std::shared_ptr<WakeState> wakeup{
        std::make_shared<WakeState>()
    };
    std::shared_ptr<DispatchTracker> dispatches{
        std::make_shared<DispatchTracker>()
    };
    routing::Router& router;
    RuntimeOptions options;

#ifdef GUNGNIR_WITH_TLS
    SslContext tls_context{
        nullptr,
        &SSL_CTX_free
    };
#endif

    std::shared_ptr<view::Engine>
        view_engine;
    std::atomic_bool running{false};
    std::atomic<NativeSocket> listener{
        invalid_socket
    };
    std::atomic<std::uint16_t> bound{0};
    std::uint64_t timer_pump_token{0};
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
        port,
        CancellationToken{}
    );
}

void Server::listen(
    std::string host,
    std::uint16_t port,
    CancellationToken cancellation
) {
    impl_->listen(
        std::move(host),
        port,
        std::move(cancellation)
    );
}

void Server::stop() noexcept {
    impl_->stop();
}

void Server::configure(
    RuntimeOptions options
) {
    impl_->configure(
        std::move(options)
    );
}

void Server::view_engine(
    std::shared_ptr<view::Engine> engine
) {
    impl_->set_view_engine(
        std::move(engine)
    );
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
