# Events

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/event.md).

## Current behavior

The native Event interface requires `name() const noexcept`. `Dispatcher::listen` registers a callback by event name with optional priority and returns a listener ID. `dispatch` invokes registered callbacks in descending priority order; `forget` removes a listener.

Dispatch is synchronous. Payloads belong to application event types.

## Limits and planned work

The target data-only immutable event syntax does not yet imply generated constructors, automatic `name()` implementations, static dispatch helpers or complete payload semantics. A bare `.gnr` declaration may still need native interface implementation.

## Implementation references

- [include/gungnir/events/event.hpp](../include/gungnir/events/event.hpp)
- [include/gungnir/events/dispatcher.hpp](../include/gungnir/events/dispatcher.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/event.md).
