#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <gungnir/http/server.hpp>
#include <gungnir/observability/observability.hpp>
#include <gungnir/routing/router.hpp>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
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

struct CertificateFiles {
    std::filesystem::path directory;
    std::filesystem::path certificate;
    std::filesystem::path key;

    ~CertificateFiles() {
        std::error_code error;

        std::filesystem::remove_all(
            directory,
            error
        );
    }
};

[[nodiscard]]
CertificateFiles make_certificate() {
    const auto nonce =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    CertificateFiles files;

    files.directory =
        std::filesystem::
            temp_directory_path() /
        (
            "gungnir-tls-" +
            std::to_string(nonce)
        );

    std::filesystem::
        create_directories(
            files.directory
        );

    files.certificate =
        files.directory /
        "server-cert.pem";

    files.key =
        files.directory /
        "server-key.pem";

    using KeyContext =
        std::unique_ptr<
            EVP_PKEY_CTX,
            decltype(
                &EVP_PKEY_CTX_free
            )
        >;

    using Key =
        std::unique_ptr<
            EVP_PKEY,
            decltype(&EVP_PKEY_free)
        >;

    using Certificate =
        std::unique_ptr<
            X509,
            decltype(&X509_free)
        >;

    using Bio =
        std::unique_ptr<
            BIO,
            decltype(&BIO_free)
        >;

    KeyContext key_context{
        EVP_PKEY_CTX_new_id(
            EVP_PKEY_RSA,
            nullptr
        ),
        &EVP_PKEY_CTX_free
    };

    assert(key_context);

    assert(
        EVP_PKEY_keygen_init(
            key_context.get()
        ) == 1
    );

    assert(
        EVP_PKEY_CTX_set_rsa_keygen_bits(
            key_context.get(),
            2048
        ) == 1
    );

    EVP_PKEY* raw_key = nullptr;

    assert(
        EVP_PKEY_keygen(
            key_context.get(),
            &raw_key
        ) == 1
    );

    Key key{
        raw_key,
        &EVP_PKEY_free
    };

    Certificate certificate{
        X509_new(),
        &X509_free
    };

    assert(certificate);

    assert(
        X509_set_version(
            certificate.get(),
            2
        ) == 1
    );

    assert(
        ASN1_INTEGER_set(
            X509_get_serialNumber(
                certificate.get()
            ),
            1
        ) == 1
    );

    assert(
        X509_gmtime_adj(
            X509_get_notBefore(
                certificate.get()
            ),
            -60
        ) != nullptr
    );

    assert(
        X509_gmtime_adj(
            X509_get_notAfter(
                certificate.get()
            ),
            24 * 60 * 60
        ) != nullptr
    );

    assert(
        X509_set_pubkey(
            certificate.get(),
            key.get()
        ) == 1
    );

    auto* subject =
        X509_get_subject_name(
            certificate.get()
        );

    assert(subject != nullptr);

    assert(
        X509_NAME_add_entry_by_txt(
            subject,
            "CN",
            MBSTRING_ASC,
            reinterpret_cast<
                const unsigned char*
            >("localhost"),
            -1,
            -1,
            0
        ) == 1
    );

    assert(
        X509_set_issuer_name(
            certificate.get(),
            subject
        ) == 1
    );

    assert(
        X509_sign(
            certificate.get(),
            key.get(),
            EVP_sha256()
        ) > 0
    );

    Bio key_file{
        BIO_new_file(
            files.key
                .string()
                .c_str(),
            "wb"
        ),
        &BIO_free
    };

    assert(key_file);

    assert(
        PEM_write_bio_PrivateKey(
            key_file.get(),
            key.get(),
            nullptr,
            nullptr,
            0,
            nullptr,
            nullptr
        ) == 1
    );

    Bio certificate_file{
        BIO_new_file(
            files.certificate
                .string()
                .c_str(),
            "wb"
        ),
        &BIO_free
    };

    assert(certificate_file);

    assert(
        PEM_write_bio_X509(
            certificate_file.get(),
            certificate.get()
        ) == 1
    );

    return files;
}

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
            "Unable to create TLS client socket"
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
            "Unable to prepare TLS client address"
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
            "Unable to connect TLS client"
        );
    }

    return socket;
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
        "TLS server did not bind a port"
    );
}

void ssl_write_all(
    SSL* ssl,
    std::string_view value
) {
    std::size_t offset = 0;

    while (offset < value.size()) {
        std::size_t written = 0;

        if (
            SSL_write_ex(
                ssl,
                value.data() + offset,
                value.size() - offset,
                &written
            ) != 1
        ) {
            throw std::runtime_error(
                "Unable to write TLS request"
            );
        }

        offset += written;
    }
}

[[nodiscard]]
std::string ssl_read_response(
    SSL* ssl
) {
    std::string response;
    char buffer[2048];

    while (
        response.find(
            "\r\n\r\ntls"
        ) ==
        std::string::npos
    ) {
        std::size_t received = 0;

        const auto status =
            SSL_read_ex(
                ssl,
                buffer,
                sizeof(buffer),
                &received
            );

        if (status != 1) {
            const auto error =
                SSL_get_error(
                    ssl,
                    status
                );

            if (
                error ==
                SSL_ERROR_ZERO_RETURN
            ) {
                break;
            }

            throw std::runtime_error(
                "Unable to read TLS response"
            );
        }

        response.append(
            buffer,
            received
        );
    }

    return response;
}

} // namespace

int main() {
    using namespace gungnir;

    SocketRuntime socket_runtime;

    const auto certificate =
        make_certificate();

    routing::Router router;

    router.get(
        "/secure",
        [] {
            return http::Response::text(
                "tls"
            );
        }
    );

    auto metric_sink =
        std::make_shared<
            observability::
                MemoryMetricSink
        >();

    observability::
        set_global_meter(
            std::make_shared<
                observability::Meter
            >(metric_sink)
        );

    http::RuntimeOptions options;

    options.shutdown_timeout =
        std::chrono::milliseconds{
            1000
        };

    options.tls =
        http::TlsOptions{
            .certificate_chain =
                certificate
                    .certificate
                    .string(),
            .private_key =
                certificate
                    .key
                    .string(),
            .alpn_protocols = {
                "http/1.1"
            }
        };

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

    SocketGuard socket{
        connect_local(port)
    };

    using Context =
        std::unique_ptr<
            SSL_CTX,
            decltype(&SSL_CTX_free)
        >;

    using Session =
        std::unique_ptr<
            SSL,
            decltype(&SSL_free)
        >;

    Context context{
        SSL_CTX_new(
            TLS_client_method()
        ),
        &SSL_CTX_free
    };

    assert(context);

    SSL_CTX_set_verify(
        context.get(),
        SSL_VERIFY_NONE,
        nullptr
    );

    Session session{
        SSL_new(
            context.get()
        ),
        &SSL_free
    };

    assert(session);

#ifdef _WIN32
    assert(
        static_cast<std::uint64_t>(
            socket.get()
        ) <=
        static_cast<std::uint64_t>(
            std::numeric_limits<int>::
                max()
        )
    );
#endif

    assert(
        SSL_set_fd(
            session.get(),
            static_cast<int>(
                socket.get()
            )
        ) == 1
    );

    constexpr unsigned char
        alpn[] = {
            8,
            'h',
            't',
            't',
            'p',
            '/',
            '1',
            '.',
            '1'
        };

    assert(
        SSL_set_alpn_protos(
            session.get(),
            alpn,
            sizeof(alpn)
        ) == 0
    );

    assert(
        SSL_connect(
            session.get()
        ) == 1
    );

    const unsigned char*
        selected = nullptr;

    unsigned int
        selected_length = 0;

    SSL_get0_alpn_selected(
        session.get(),
        &selected,
        &selected_length
    );

    assert(
        std::string_view{
            reinterpret_cast<
                const char*
            >(selected),
            selected_length
        } ==
        "http/1.1"
    );

    ssl_write_all(
        session.get(),
        "GET /secure HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n"
    );

    const auto response =
        ssl_read_response(
            session.get()
        );

    assert(
        response.find(
            "HTTP/1.1 200 OK\r\n"
        ) == 0
    );

    assert(
        response.ends_with(
            "\r\n\r\ntls"
        )
    );

    static_cast<void>(
        SSL_shutdown(
            session.get()
        )
    );

    server.stop();
    server_thread.join();

    if (server_error) {
        std::rethrow_exception(
            server_error
        );
    }

    const auto points =
        metric_sink->points();

    bool saw_handshake = false;

    for (const auto& point : points) {
        if (
            point.name ==
            "http.server.tls.handshake.count" &&
            point.attributes.at(
                "outcome"
            ) ==
            "ok"
        ) {
            saw_handshake = true;
            break;
        }
    }

    assert(saw_handshake);

    observability::
        set_global_meter(
            nullptr
        );

    return 0;
}
