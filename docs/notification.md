# Notifications

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/notification.md).

## Current behavior

Structured notification declarations retain typed constructor data and require `via(Recipient recipient)`. `bind(recipient)` creates an adapter implementing native `notifications::Notification`, preserving the qualified name and recipient-specific channels. Typed `to_mail()` / `to_database()` accessors forward declared recipient payload methods.

## Limits and planned work

Channels still need explicit native registration. Built-in database/mail channel delivery adapters and automatic recipient addressing are not supplied by this compiler.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/notifications/notification.hpp](../include/gungnir/notifications/notification.hpp)
- [include/gungnir/notifications/channel.hpp](../include/gungnir/notifications/channel.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/notification.md).
