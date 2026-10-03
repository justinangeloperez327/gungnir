# Notifications

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

## Current behavior

Structured notification declarations retain typed constructor data and require public synchronous `via(Recipient recipient) -> List<string>`, where the recipient is a non-optional model. `bind(recipient)` creates an adapter implementing native `notifications::Notification`, preserving the qualified name and recipient-specific channels. `toMail` must return a structured mail declaration and `toDatabase` must return `Json`; both use the same recipient contract as `via`. Invalid surfaces are rejected with `GNR2306`.

## Limits and planned work

Channels still need explicit native registration. Built-in database/mail channel delivery adapters and automatic recipient addressing are not supplied by this compiler.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/notifications/notification.hpp](../include/gungnir/notifications/notification.hpp)
- [include/gungnir/notifications/channel.hpp](../include/gungnir/notifications/channel.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/notification.md).
