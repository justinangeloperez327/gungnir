# Notifications

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/notification.md).

## Current behavior

Native Notification requires `name()` and `channels()`. A Channel sends to a recipient string. `notifications::Manager::channel` registers channel instances and `send` dispatches to the names returned by the notification.

An unconfigured channel raises a logic error. Channel instances must outlive the manager's use of them.

## Limits and planned work

Mail/database/SMS/push action methods, user `notify` shorthand and automatic queued delivery are target APIs. Applications currently provide concrete channel implementations and payload composition.

## Implementation references

- [include/gungnir/notifications/notification.hpp](../include/gungnir/notifications/notification.hpp)
- [include/gungnir/notifications/channel.hpp](../include/gungnir/notifications/channel.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/notification.md).
