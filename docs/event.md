# Events

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/event.md).

## Current behavior

In the structured frontend, events contain typed data fields and generate immutable constructors, a qualified event name and the native `events::Event` interface. Event methods and metadata are rejected.

## Limits and planned work

General event serialization and externally supplied custom event bases remain native concerns.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/events/event.hpp](../include/gungnir/events/event.hpp)
- [include/gungnir/events/dispatcher.hpp](../include/gungnir/events/dispatcher.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/event.md).
