# Mail

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/mail.md).

## Current behavior

Native Message supports sender, recipients, cc, bcc, subject, plain text and HTML. Mailer wraps a Transport and sends synchronously, recording tracing and metrics. MemoryTransport is available for tests; optional SMTP transport provides network delivery.

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

Structured `mail` declarations generate typed data constructors and `message()` composition from parameterless subject/text/html methods. View-response content still needs native rendering integration before sending.

## Limits and planned work

Target `mail` action declarations, template composition, attachments, custom headers and fluent static sending are not all fields/methods of the current Message class. Sending does not automatically enqueue work.

## Implementation references

- [include/gungnir/mail/message.hpp](../include/gungnir/mail/message.hpp)
- [include/gungnir/mail/transport.hpp](../include/gungnir/mail/transport.hpp)
- [include/gungnir/mail/memory_transport.hpp](../include/gungnir/mail/memory_transport.hpp)
- [include/gungnir/mail/smtp_transport.hpp](../include/gungnir/mail/smtp_transport.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/mail.md).
