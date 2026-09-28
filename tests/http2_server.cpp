#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <gungnir/http/server.hpp>
#include <gungnir/routing/router.hpp>

namespace {

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
            "gungnir-http2-" +
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

    if (
        !key_context ||
        EVP_PKEY_keygen_init(
            key_context.get()
        ) != 1 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(
            key_context.get(),
            2048
        ) != 1
    ) {
        throw std::runtime_error(
            "Unable to initialize HTTP/2 test key"
        );
    }

    EVP_PKEY* raw_key = nullptr;

    if (
        EVP_PKEY_keygen(
            key_context.get(),
            &raw_key
        ) != 1
    ) {
        throw std::runtime_error(
            "Unable to generate HTTP/2 test key"
        );
    }

    Key key{
        raw_key,
        &EVP_PKEY_free
    };

    Certificate certificate{
        X509_new(),
        &X509_free
    };

    if (!certificate) {
        throw std::runtime_error(
            "Unable to allocate HTTP/2 test certificate"
        );
    }

    if (
        X509_set_version(
            certificate.get(),
            2
        ) != 1 ||
        ASN1_INTEGER_set(
            X509_get_serialNumber(
                certificate.get()
            ),
            2
        ) != 1 ||
        X509_gmtime_adj(
            X509_get_notBefore(
                certificate.get()
            ),
            -60
        ) == nullptr ||
        X509_gmtime_adj(
            X509_get_notAfter(
                certificate.get()
            ),
            24 * 60 * 60
        ) == nullptr ||
        X509_set_pubkey(
            certificate.get(),
            key.get()
        ) != 1
    ) {
        throw std::runtime_error(
            "Unable to configure HTTP/2 test certificate"
        );
    }

    auto* subject =
        X509_get_subject_name(
            certificate.get()
        );

    if (
        subject == nullptr ||
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
        ) != 1 ||
        X509_set_issuer_name(
            certificate.get(),
            subject
        ) != 1 ||
        X509_sign(
            certificate.get(),
            key.get(),
            EVP_sha256()
        ) <= 0
    ) {
        throw std::runtime_error(
            "Unable to sign HTTP/2 test certificate"
        );
    }

    Bio key_file{
        BIO_new_file(
            files.key
                .string()
                .c_str(),
            "wb"
        ),
        &BIO_free
    };

    Bio certificate_file{
        BIO_new_file(
            files.certificate
                .string()
                .c_str(),
            "wb"
        ),
        &BIO_free
    };

    if (
        !key_file ||
        !certificate_file ||
        PEM_write_bio_PrivateKey(
            key_file.get(),
            key.get(),
            nullptr,
            nullptr,
            0,
            nullptr,
            nullptr
        ) != 1 ||
        PEM_write_bio_X509(
            certificate_file.get(),
            certificate.get()
        ) != 1
    ) {
        throw std::runtime_error(
            "Unable to write HTTP/2 test certificate files"
        );
    }

    return files;
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
        "HTTP/2 server did not bind"
    );
}

std::size_t write_body(
    char* data,
    std::size_t size,
    std::size_t count,
    void* userdata
) {
    auto& body =
        *static_cast<std::string*>(
            userdata
        );

    const auto bytes =
        size * count;

    body.append(
        data,
        bytes
    );

    return bytes;
}

class CurlRuntime {
public:
    CurlRuntime() {
        if (
            curl_global_init(
                CURL_GLOBAL_DEFAULT
            ) != CURLE_OK
        ) {
            throw std::runtime_error(
                "Unable to initialize curl"
            );
        }
    }

    ~CurlRuntime() {
        curl_global_cleanup();
    }
};

using CurlHandle =
    std::unique_ptr<
        CURL,
        decltype(&curl_easy_cleanup)
    >;

} // namespace

int main() {
    using namespace gungnir;

    CurlRuntime curl_runtime;

    const auto* version =
        curl_version_info(
            CURLVERSION_NOW
        );

    if (
        version == nullptr ||
        (
            version->features &
            CURL_VERSION_HTTP2
        ) == 0
    ) {
        throw std::runtime_error(
            "HTTP/2 integration test requires a curl build with HTTP/2"
        );
    }

    const auto certificate =
        make_certificate();

    routing::Router router;

    router.get(
        "/h2",
        [](
            http::Request& request
        ) {
            assert(
                request.secure()
            );

            return
                http::Response::text(
                    "h2:" +
                    std::string{
                        request.header(
                            "x-test"
                        )
                    }
                );
        }
    );

    http::RuntimeOptions options;

    options.shutdown_timeout =
        std::chrono::seconds{1};

    options.tls =
        http::TlsOptions{
            .certificate_chain =
                certificate
                    .certificate
                    .string(),
            .private_key =
                certificate
                    .key
                    .string()
        };

    options.http2 =
        http::Http2Options{
            .max_concurrent_streams = 32,
            .max_header_list_bytes =
                16U * 1024U
        };

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

    CurlHandle handle{
        curl_easy_init(),
        &curl_easy_cleanup
    };

    if (!handle) {
        server.stop();
        server_thread.join();

        throw std::runtime_error(
            "Unable to allocate curl handle"
        );
    }

    const auto url =
        "https://127.0.0.1:" +
        std::to_string(port) +
        "/h2";

    std::string body;

    curl_easy_setopt(
        handle.get(),
        CURLOPT_URL,
        url.c_str()
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_HTTP_VERSION,
        CURL_HTTP_VERSION_2TLS
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_SSL_VERIFYPEER,
        0L
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_SSL_VERIFYHOST,
        0L
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_WRITEFUNCTION,
        &write_body
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_WRITEDATA,
        &body
    );

    curl_easy_setopt(
        handle.get(),
        CURLOPT_TIMEOUT_MS,
        3000L
    );

    curl_slist* raw_headers =
        nullptr;

    raw_headers =
        curl_slist_append(
            raw_headers,
            "x-test: routed"
        );

    std::unique_ptr<
        curl_slist,
        decltype(&curl_slist_free_all)
    > headers{
        raw_headers,
        &curl_slist_free_all
    };

    curl_easy_setopt(
        handle.get(),
        CURLOPT_HTTPHEADER,
        headers.get()
    );

    const auto status =
        curl_easy_perform(
            handle.get()
        );

    long response_code = 0;
    long http_version = 0;

    curl_easy_getinfo(
        handle.get(),
        CURLINFO_RESPONSE_CODE,
        &response_code
    );

    curl_easy_getinfo(
        handle.get(),
        CURLINFO_HTTP_VERSION,
        &http_version
    );

    server.stop();
    server_thread.join();

    if (server_error) {
        std::rethrow_exception(
            server_error
        );
    }

    if (status != CURLE_OK) {
        throw std::runtime_error(
            std::string{
                "HTTP/2 request failed: "
            } +
            curl_easy_strerror(status)
        );
    }

    assert(response_code == 200);
    assert(
        http_version ==
        CURL_HTTP_VERSION_2_0
    );
    assert(body == "h2:routed");

    return 0;
}
