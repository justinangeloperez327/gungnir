#include <cassert>
#include <cstdlib>
#include <string>

#include <gungnir/mail/smtp_transport.hpp>

int main() {
    gungnir::mail::SmtpSettings
        settings;

    if (
        const auto* host =
            std::getenv(
                "GUNGNIR_SMTP_HOST"
            )
    ) {
        settings.host = host;
    }

    if (
        const auto* port =
            std::getenv(
                "GUNGNIR_SMTP_PORT"
            )
    ) {
        settings.port =
            static_cast<std::uint16_t>(
                std::stoi(port)
            );
    }

    settings.security =
        gungnir::mail::
            SmtpSecurity::none;

    gungnir::mail::SmtpTransport
        transport{
            settings
        };

    assert(
        transport.settings().port ==
        settings.port
    );

    gungnir::mail::Message message;

    message
        .from({
            "package@example.test",
            "Package Consumer"
        })
        .to({
            "recipient@example.test",
            "Recipient"
        })
        .subject(
            "Gungnir SMTP Package Consumer"
        )
        .text(
            "Installed package delivery"
        );

    transport.send(message);

    return 0;
}
