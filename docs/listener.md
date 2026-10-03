# Listeners

> **Status: Gungnir 1.0 current contract.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/listener.md).

## Current behavior

Structured listeners require `handle(EventType event)`. Generated `register_listener(dispatcher, shared_ptr<Listener>, priority)` binds the typed event to native dispatch. Async handlers register through `listen_async` and execute through `dispatch_async`; synchronous dispatch rejects async listeners before executing callbacks.

## Limits and planned work

The dispatcher must outlive a pending dispatch, and the event must remain alive until asynchronous dispatch completes. Queueing, persistence and distributed event delivery remain separate services.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/events/dispatcher.hpp](../include/gungnir/events/dispatcher.hpp)
- [include/gungnir/core/framework_artifacts.hpp](../include/gungnir/core/framework_artifacts.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/listener.md).
