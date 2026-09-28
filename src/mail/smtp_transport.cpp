#include <gungnir/mail/smtp_transport.hpp>

#include <array>
#include <chrono>
#include <ctime>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <curl/curl.h>

#include <gungnir/security/random.hpp>
#include <gungnir/security/security.hpp>

namespace gungnir::mail {

namespace {

class CurlRuntime {
public:
    CurlRuntime() {
        const auto status =
            curl_global_init(
                CURL_GLOBAL_DEFAULT
            );

        if (status != CURLE_OK) {
            throw std::runtime_error(
                "Unable to initialize libcurl"
            );
        }
    }

    ~CurlRuntime() {
        curl_global_cleanup();
    }
};

void ensure_curl_runtime() {
    static CurlRuntime runtime;
    static_cast<void>(runtime);
}

using EasyHandle =
    std::unique_ptr<
        CURL,
        decltype(&curl_easy_cleanup)
    >;

using MimeHandle =
    std::unique_ptr<
        curl_mime,
        decltype(&curl_mime_free)
    >;

using HeaderList =
    std::unique_ptr<
        curl_slist,
        decltype(&curl_slist_free_all)
    >;

void check(
    CURLcode status,
    std::string_view operation
) {
    if (status == CURLE_OK) {
        return;
    }

    throw std::runtime_error(
        "SMTP " +
        std::string{operation} +
        " failed: " +
        curl_easy_strerror(status)
    );
}

void append(
    curl_slist*& list,
    const std::string& value
) {
    auto* updated =
        curl_slist_append(
            list,
            value.c_str()
        );

    if (updated == nullptr) {
        throw std::bad_alloc{};
    }

    list = updated;
}

[[nodiscard]]
bool valid_mailbox(
    std::string_view value
) noexcept {
    const auto at =
        value.find('@');

    if (
        value.empty() ||
        at == std::string_view::npos ||
        at == 0 ||
        at + 1 >= value.size() ||
        value.find(
            '@',
            at + 1
        ) != std::string_view::npos
    ) {
        return false;
    }

    for (const auto character : value) {
        const auto byte =
            static_cast<unsigned char>(
                character
            );

        if (
            byte <= 0x20 ||
            byte >= 0x7f ||
            character == '<' ||
            character == '>' ||
            character == ',' ||
            character == ';'
        ) {
            return false;
        }
    }

    return true;
}

[[nodiscard]]
bool valid_header_text(
    std::string_view value
) noexcept {
    if (
        !security::valid_header_value(
            value
        )
    ) {
        return false;
    }

    for (const auto character : value) {
        const auto byte =
            static_cast<unsigned char>(
                character
            );

        if (
            byte < 0x20 ||
            byte == 0x7f
        ) {
            return false;
        }
    }

    return true;
}

void validate_address(
    const Address& address,
    std::string_view role
) {
    if (
        !valid_mailbox(
            address.email
        )
    ) {
        throw std::invalid_argument(
            "SMTP " +
            std::string{role} +
            " address is invalid"
        );
    }

    if (
        !valid_header_text(
            address.name
        )
    ) {
        throw std::invalid_argument(
            "SMTP " +
            std::string{role} +
            " display name contains invalid header characters"
        );
    }
}

[[nodiscard]]
std::string display_address(
    const Address& address
) {
    if (address.name.empty()) {
        return
            "<" +
            address.email +
            ">";
    }

    std::string name;
    name.reserve(
        address.name.size() + 8
    );

    for (
        const auto character :
        address.name
    ) {
        if (
            character == '\\' ||
            character == '"'
        ) {
            name.push_back('\\');
        }

        name.push_back(character);
    }

    return
        "\"" +
        name +
        "\" <" +
        address.email +
        ">";
}

[[nodiscard]]
std::string joined_addresses(
    const std::vector<Address>& addresses
) {
    std::string output;

    for (
        std::size_t index = 0;
        index < addresses.size();
        ++index
    ) {
        if (index != 0) {
            output += ", ";
        }

        output +=
            display_address(
                addresses[index]
            );
    }

    return output;
}

[[nodiscard]]
std::string envelope_address(
    const Address& address
) {
    return
        "<" +
        address.email +
        ">";
}

[[nodiscard]]
std::string message_date() {
    const auto now =
        std::chrono::system_clock::now();

    const auto time =
        std::chrono::system_clock::
            to_time_t(now);

    std::tm utc{};

#ifdef _WIN32
    if (
        gmtime_s(
            &utc,
            &time
        ) != 0
    ) {
        throw std::runtime_error(
            "Unable to create SMTP Date header"
        );
    }
#else
    if (
        gmtime_r(
            &time,
            &utc
        ) == nullptr
    ) {
        throw std::runtime_error(
            "Unable to create SMTP Date header"
        );
    }
#endif

    std::array<char, 64> buffer{};

    if (
        std::strftime(
            buffer.data(),
            buffer.size(),
            "%a, %d %b %Y %H:%M:%S +0000",
            &utc
        ) == 0
    ) {
        throw std::runtime_error(
            "Unable to format SMTP Date header"
        );
    }

    return
        std::string{
            buffer.data()
        };
}

[[nodiscard]]
std::string message_id(
    const Address& sender
) {
    const auto at =
        sender.email.rfind('@');

    const auto domain =
        at == std::string::npos
            ? std::string_view{
                "gungnir.local"
              }
            : std::string_view{
                sender.email
              }.substr(
                at + 1
              );

    return
        "<" +
        security::random_token(16) +
        "@" +
        std::string{domain} +
        ">";
}

[[nodiscard]]
std::string smtp_url(
    const SmtpSettings& settings
) {
    const auto scheme =
        settings.security ==
            SmtpSecurity::implicit_tls
            ? "smtps://"
            : "smtp://";

    return
        std::string{scheme} +
        settings.host +
        ":" +
        std::to_string(
            settings.port
        );
}

void configure_settings(
    CURL* curl,
    const SmtpSettings& settings
) {
    check(
        curl_easy_setopt(
            curl,
            CURLOPT_URL,
            smtp_url(settings).c_str()
        ),
        "server URL configuration"
    );

    check(
        curl_easy_setopt(
            curl,
            CURLOPT_CONNECTTIMEOUT_MS,
            static_cast<long>(
                settings
                    .connect_timeout
                    .count()
            )
        ),
        "connect timeout configuration"
    );

    check(
        curl_easy_setopt(
            curl,
            CURLOPT_TIMEOUT_MS,
            static_cast<long>(
                settings
                    .transfer_timeout
                    .count()
            )
        ),
        "transfer timeout configuration"
    );

    check(
        curl_easy_setopt(
            curl,
            CURLOPT_NOSIGNAL,
            1L
        ),
        "thread-safety configuration"
    );

    if (
        settings.security ==
        SmtpSecurity::start_tls
    ) {
        check(
            curl_easy_setopt(
                curl,
                CURLOPT_USE_SSL,
                CURLUSESSL_ALL
            ),
            "STARTTLS configuration"
        );
    }

    if (
        settings.security !=
        SmtpSecurity::none
    ) {
        check(
            curl_easy_setopt(
                curl,
                CURLOPT_SSL_VERIFYPEER,
                settings.verify_peer
                    ? 1L
                    : 0L
            ),
            "TLS peer verification configuration"
        );

        check(
            curl_easy_setopt(
                curl,
                CURLOPT_SSL_VERIFYHOST,
                settings.verify_host
                    ? 2L
                    : 0L
            ),
            "TLS host verification configuration"
        );
    }

    if (settings.username) {
        check(
            curl_easy_setopt(
                curl,
                CURLOPT_USERNAME,
                settings
                    .username
                    ->c_str()
            ),
            "username configuration"
        );

        check(
            curl_easy_setopt(
                curl,
                CURLOPT_PASSWORD,
                settings
                    .password
                    ->c_str()
            ),
            "password configuration"
        );
    }
}

void add_body(
    CURL* curl,
    curl_mime* root,
    const Message& message
) {
    const auto& text =
        message.text_body();

    const auto& html =
        message.html_body();

    if (
        !text.empty() &&
        !html.empty()
    ) {
        auto* alternative =
            curl_mime_init(curl);

        if (alternative == nullptr) {
            throw std::bad_alloc{};
        }

        auto alternative_guard =
            MimeHandle{
                alternative,
                &curl_mime_free
            };

        auto* text_part =
            curl_mime_addpart(
                alternative
            );

        if (text_part == nullptr) {
            throw std::bad_alloc{};
        }

        check(
            curl_mime_data(
                text_part,
                text.data(),
                text.size()
            ),
            "text body configuration"
        );

        check(
            curl_mime_type(
                text_part,
                "text/plain; charset=utf-8"
            ),
            "text content type configuration"
        );

        auto* html_part =
            curl_mime_addpart(
                alternative
            );

        if (html_part == nullptr) {
            throw std::bad_alloc{};
        }

        check(
            curl_mime_data(
                html_part,
                html.data(),
                html.size()
            ),
            "HTML body configuration"
        );

        check(
            curl_mime_type(
                html_part,
                "text/html; charset=utf-8"
            ),
            "HTML content type configuration"
        );

        auto* container =
            curl_mime_addpart(root);

        if (container == nullptr) {
            throw std::bad_alloc{};
        }

        check(
            curl_mime_subparts(
                container,
                alternative
            ),
            "alternative MIME configuration"
        );

        alternative_guard.release();

        check(
            curl_mime_type(
                container,
                "multipart/alternative"
            ),
            "alternative content type configuration"
        );

        return;
    }

    auto* part =
        curl_mime_addpart(root);

    if (part == nullptr) {
        throw std::bad_alloc{};
    }

    const auto& body =
        html.empty()
            ? text
            : html;

    check(
        curl_mime_data(
            part,
            body.data(),
            body.size()
        ),
        "message body configuration"
    );

    check(
        curl_mime_type(
            part,
            html.empty()
                ? "text/plain; charset=utf-8"
                : "text/html; charset=utf-8"
        ),
        "message content type configuration"
    );
}

} // namespace

class SmtpTransport::Impl {
public:
    explicit Impl(
        SmtpSettings value
    )
        : settings(
            std::move(value)
          ) {
        validate();
        ensure_curl_runtime();
    }

    void send(
        const Message& message
    ) const {
        validate_message(
            message
        );

        EasyHandle curl{
            curl_easy_init(),
            &curl_easy_cleanup
        };

        if (!curl) {
            throw std::runtime_error(
                "Unable to allocate libcurl SMTP handle"
            );
        }

        configure_settings(
            curl.get(),
            settings
        );

        const auto sender =
            envelope_address(
                message.sender()
            );

        check(
            curl_easy_setopt(
                curl.get(),
                CURLOPT_MAIL_FROM,
                sender.c_str()
            ),
            "sender configuration"
        );

        curl_slist* raw_recipients =
            nullptr;

        try {
            for (
                const auto& address :
                message.recipients()
            ) {
                append(
                    raw_recipients,
                    envelope_address(
                        address
                    )
                );
            }

            for (
                const auto& address :
                message.cc_recipients()
            ) {
                append(
                    raw_recipients,
                    envelope_address(
                        address
                    )
                );
            }

            for (
                const auto& address :
                message.bcc_recipients()
            ) {
                append(
                    raw_recipients,
                    envelope_address(
                        address
                    )
                );
            }
        } catch (...) {
            curl_slist_free_all(
                raw_recipients
            );
            throw;
        }

        HeaderList recipients{
            raw_recipients,
            &curl_slist_free_all
        };

        check(
            curl_easy_setopt(
                curl.get(),
                CURLOPT_MAIL_RCPT,
                recipients.get()
            ),
            "recipient configuration"
        );

        curl_slist* raw_headers =
            nullptr;

        try {
            append(
                raw_headers,
                "From: " +
                    display_address(
                        message.sender()
                    )
            );

            if (
                !message.recipients()
                    .empty()
            ) {
                append(
                    raw_headers,
                    "To: " +
                        joined_addresses(
                            message
                                .recipients()
                        )
                );
            } else if (
                message.cc_recipients()
                    .empty()
            ) {
                append(
                    raw_headers,
                    "To: undisclosed-recipients:;"
                );
            }

            if (
                !message.cc_recipients()
                    .empty()
            ) {
                append(
                    raw_headers,
                    "Cc: " +
                        joined_addresses(
                            message
                                .cc_recipients()
                        )
                );
            }

            append(
                raw_headers,
                "Subject: " +
                    message.subject_line()
            );

            append(
                raw_headers,
                "Date: " +
                    message_date()
            );

            append(
                raw_headers,
                "Message-ID: " +
                    message_id(
                        message.sender()
                    )
            );
        } catch (...) {
            curl_slist_free_all(
                raw_headers
            );
            throw;
        }

        HeaderList headers{
            raw_headers,
            &curl_slist_free_all
        };

        check(
            curl_easy_setopt(
                curl.get(),
                CURLOPT_HTTPHEADER,
                headers.get()
            ),
            "message header configuration"
        );

        MimeHandle mime{
            curl_mime_init(
                curl.get()
            ),
            &curl_mime_free
        };

        if (!mime) {
            throw std::bad_alloc{};
        }

        add_body(
            curl.get(),
            mime.get(),
            message
        );

        check(
            curl_easy_setopt(
                curl.get(),
                CURLOPT_MIMEPOST,
                mime.get()
            ),
            "MIME upload configuration"
        );

        std::array<char, CURL_ERROR_SIZE>
            error{};

        check(
            curl_easy_setopt(
                curl.get(),
                CURLOPT_ERRORBUFFER,
                error.data()
            ),
            "error buffer configuration"
        );

        const auto status =
            curl_easy_perform(
                curl.get()
            );

        if (status != CURLE_OK) {
            const auto detail =
                error.front() == '\0'
                    ? std::string{
                        curl_easy_strerror(
                            status
                        )
                      }
                    : std::string{
                        error.data()
                      };

            throw std::runtime_error(
                "SMTP delivery failed: " +
                detail
            );
        }
    }

    SmtpSettings settings;

private:
    void validate() const {
        if (settings.host.empty()) {
            throw std::invalid_argument(
                "SMTP host cannot be empty"
            );
        }

        if (settings.port == 0) {
            throw std::invalid_argument(
                "SMTP port must be greater than zero"
            );
        }

        if (
            settings
                .connect_timeout
                .count() <= 0 ||
            settings
                .transfer_timeout
                .count() <= 0
        ) {
            throw std::invalid_argument(
                "SMTP timeouts must be greater than zero"
            );
        }

        if (
            settings.username
                .has_value() !=
            settings.password
                .has_value()
        ) {
            throw std::invalid_argument(
                "SMTP username and password must be configured together"
            );
        }

        if (
            settings.username &&
            settings.security ==
                SmtpSecurity::none
        ) {
            throw std::invalid_argument(
                "SMTP credentials require TLS"
            );
        }
    }

    static void validate_message(
        const Message& message
    ) {
        validate_address(
            message.sender(),
            "sender"
        );

        if (
            message.recipients()
                .empty() &&
            message.cc_recipients()
                .empty() &&
            message.bcc_recipients()
                .empty()
        ) {
            throw std::invalid_argument(
                "SMTP message requires at least one recipient"
            );
        }

        for (
            const auto& address :
            message.recipients()
        ) {
            validate_address(
                address,
                "recipient"
            );
        }

        for (
            const auto& address :
            message.cc_recipients()
        ) {
            validate_address(
                address,
                "CC recipient"
            );
        }

        for (
            const auto& address :
            message.bcc_recipients()
        ) {
            validate_address(
                address,
                "BCC recipient"
            );
        }

        if (
            !valid_header_text(
                message.subject_line()
            )
        ) {
            throw std::invalid_argument(
                "SMTP subject contains invalid header characters"
            );
        }
    }
};

SmtpTransport::SmtpTransport(
    SmtpSettings settings
)
    : impl_(
        std::make_unique<Impl>(
            std::move(settings)
        )
      ) {}

SmtpTransport::~SmtpTransport() =
    default;

SmtpTransport::SmtpTransport(
    SmtpTransport&&
) noexcept = default;

SmtpTransport& SmtpTransport::operator=(
    SmtpTransport&&
) noexcept = default;

void SmtpTransport::send(
    const Message& message
) {
    impl_->send(message);
}

const SmtpSettings&
SmtpTransport::settings()
    const noexcept {
    return impl_->settings;
}

} // namespace gungnir::mail
