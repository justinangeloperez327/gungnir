# Mail

Gungnir keeps message composition separate from delivery.

`mail::Message` defines the sender, recipients, subject, and optional text/HTML bodies. `mail::Mailer` sends through a `mail::Transport`.

## In-memory transport

`MemoryTransport` remains available for tests and local assertions:

```cpp
gungnir::mail::MemoryTransport transport;
gungnir::mail::Mailer mailer{transport};

gungnir::mail::Message message;
message
    .from({"sender@example.com", "Sender"})
    .to({"user@example.com", "User"})
    .subject("Welcome")
    .text("Hello");

mailer.send(message);
```

## SMTP transport

Build Gungnir with `GUNGNIR_WITH_SMTP=ON` to enable the optional libcurl-backed SMTP adapter:

```cpp
gungnir::mail::SmtpSettings settings;
settings.host = "smtp.example.com";
settings.port = 587;
settings.security =
    gungnir::mail::SmtpSecurity::start_tls;
settings.username = "mailer";
settings.password = "secret";

gungnir::mail::SmtpTransport transport{
    settings
};

gungnir::mail::Mailer mailer{
    transport
};
```

Supported transport modes are:

- `start_tls` — `smtp://` with mandatory STARTTLS;
- `implicit_tls` — `smtps://`; and
- `none` — plaintext SMTP for explicitly non-authenticated local/test relays.

Gungnir refuses username/password authentication when transport security is `none`. TLS peer and hostname verification are enabled by default.

The adapter delegates SMTP protocol handling, TLS, authentication, recipient commands, and MIME upload to libcurl. Gungnir validates sender/recipient mailbox values and rejects control characters in display names and subjects before they become mail headers.

Messages may contain text, HTML, or both. When both bodies are present they are sent as MIME alternatives. `To`, `Cc`, and `Bcc` recipients are all SMTP envelope recipients, but Bcc addresses are intentionally omitted from visible message headers.

The transport adds Date and Message-ID headers automatically.

## Delivery failures

`SmtpTransport::send` throws when configuration is invalid or libcurl reports a delivery failure. The transport does not silently downgrade STARTTLS.

Application retry policy should normally be handled by the queue layer rather than by retrying indefinitely inside the SMTP transport itself.

## Remaining mail work

Attachments, provider-specific HTTP mail APIs, DKIM signing, and application-level delivery telemetry remain separate adapter/features. SMTP delivery itself is a concrete production transport when configured against a production SMTP provider.
