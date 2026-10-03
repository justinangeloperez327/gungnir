# Mail

> **Status: Gungnir 1.0 current contract.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/mail.md).

## Current behavior

Native Message supports sender, recipients, cc, bcc, subject, plain text, HTML, reply-to and in-memory attachments. Mailer wraps a Transport and sends synchronously, recording tracing and metrics. MemoryTransport is available for tests; optional SMTP transport provides network delivery.

Enable the SMTP adapter with `-DGUNGNIR_WITH_SMTP=ON` and link `gungnir::smtp`. Configure the concrete transport before constructing the Mailer.

## Example

```cpp
gungnir::mail::Message message;
message.from({"sender@example.com", "Gungnir"})
       .to({"recipient@example.com", "Recipient"})
       .subject("Welcome")
       .text("Your account is ready.");
// Send through a Mailer configured with a Transport.
```

Structured `mail` declarations generate typed data constructors and `message()` composition from synchronous parameterless `subject`, `text`, and `html` methods. A `content() -> Response` method is also supported and its response body becomes the HTML body. `html()` and `content()` cannot both define the HTML source.

## Limits and planned work

Target `mail` action declarations, additional template composition, custom headers and fluent static sending are not all fields/methods of the current Message class. Sending does not automatically enqueue work. Framework composition violations are rejected by `gungnirc --check` with `GNR2307`.

## Implementation references

- [include/gungnir/mail/message.hpp](../include/gungnir/mail/message.hpp)
- [include/gungnir/mail/transport.hpp](../include/gungnir/mail/transport.hpp)
- [include/gungnir/mail/memory_transport.hpp](../include/gungnir/mail/memory_transport.hpp)
- [include/gungnir/mail/smtp_transport.hpp](../include/gungnir/mail/smtp_transport.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/mail.md).
