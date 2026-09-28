#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include <curl/curl.h>

#include <gungnir/mail/smtp_transport.hpp>

namespace {

std::string env(
    const char* name,
    std::string fallback
) {
    if (
        const auto* value =
            std::getenv(name)
    ) {
        return value;
    }

    return fallback;
}

std::uint16_t smtp_port() {
    return static_cast<std::uint16_t>(
        std::stoi(
            env(
                "GUNGNIR_SMTP_PORT",
                "1025"
            )
        )
    );
}

std::string api_url() {
    return
        "http://" +
        env(
            "GUNGNIR_MAILPIT_HOST",
            "127.0.0.1"
        ) +
        ":" +
        env(
            "GUNGNIR_MAILPIT_PORT",
            "8025"
        ) +
        "/api/v1/messages?limit=20";
}

std::size_t append_body(
    char* data,
    std::size_t size,
    std::size_t count,
    void* context
) {
    const auto bytes =
        size * count;

    static_cast<std::string*>(
        context
    )->append(
        data,
        bytes
    );

    return bytes;
}

std::string messages() {
    auto* curl =
        curl_easy_init();

    assert(curl != nullptr);

    std::string body;

    assert(
        curl_easy_setopt(
            curl,
            CURLOPT_URL,
            api_url().c_str()
        ) == CURLE_OK
    );

    assert(
        curl_easy_setopt(
            curl,
            CURLOPT_WRITEFUNCTION,
            &append_body
        ) == CURLE_OK
    );

    assert(
        curl_easy_setopt(
            curl,
            CURLOPT_WRITEDATA,
            &body
        ) == CURLE_OK
    );

    assert(
        curl_easy_setopt(
            curl,
            CURLOPT_TIMEOUT_MS,
            5000L
        ) == CURLE_OK
    );

    const auto result =
        curl_easy_perform(curl);

    curl_easy_cleanup(curl);

    assert(result == CURLE_OK);

    return body;
}

} // namespace

int main() {
    using namespace gungnir;

    mail::SmtpSettings settings;
    settings.host =
        env(
            "GUNGNIR_SMTP_HOST",
            "127.0.0.1"
        );

    settings.port =
        smtp_port();

    settings.security =
        mail::SmtpSecurity::none;

    mail::SmtpTransport transport{
        settings
    };

    mail::Mailer mailer{
        transport
    };

    mail::Message message;

    message
        .from({
            "sender@example.test",
            "Gungnir Sender"
        })
        .to({
            "user@example.test",
            "Primary User"
        })
        .cc({
            "copy@example.test",
            "Copy User"
        })
        .bcc({
            "audit@example.test",
            "Audit User"
        })
        .subject(
            "Gungnir SMTP Integration"
        )
        .text(
            "Plain Gungnir mail"
        )
        .html(
            "<p>HTML <strong>Gungnir</strong> mail</p>"
        );

    mailer.send(message);

    const auto received =
        messages();

    assert(
        received.find(
            "Gungnir SMTP Integration"
        ) != std::string::npos
    );

    assert(
        received.find(
            "sender@example.test"
        ) != std::string::npos
    );

    bool injection_rejected = false;

    try {
        mail::Message injected;

        injected
            .from({
                "sender@example.test",
                "Sender"
            })
            .to({
                "user@example.test",
                "User"
            })
            .subject(
                "safe\r\nBcc: attacker@example.test"
            )
            .text("body");

        mailer.send(injected);
    } catch (
        const std::invalid_argument&
    ) {
        injection_rejected = true;
    }

    assert(injection_rejected);

    bool plaintext_auth_rejected =
        false;

    try {
        auto insecure = settings;
        insecure.username =
            "user";
        insecure.password =
            "password";

        mail::SmtpTransport rejected{
            insecure
        };

        static_cast<void>(
            rejected
        );
    } catch (
        const std::invalid_argument&
    ) {
        plaintext_auth_rejected = true;
    }

    assert(
        plaintext_auth_rejected
    );

    return 0;
}
