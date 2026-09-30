# Listeners

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/listener.md).

## Current behavior

Runtime listeners are `std::function<void(const Event&)>` callbacks registered with an explicit event name. The framework `Listener` base has a virtual destructor, but does not itself register or invoke `handle` methods.

Keep the dispatcher and captured dependencies alive while listeners remain registered; remove registrations when their owners stop.

## Limits and planned work

Automatic event-type inference, async listener dispatch, dependency injection and queued listeners in the language design need explicit lowering/runtime integration. `async` does not automatically queue an event handler.

## Implementation references

- [include/gungnir/events/dispatcher.hpp](../include/gungnir/events/dispatcher.hpp)
- [include/gungnir/core/framework_artifacts.hpp](../include/gungnir/core/framework_artifacts.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/listener.md).
