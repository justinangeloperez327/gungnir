# Application Lifecycle

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/application-lifecycle.md).

## Current behavior

The implemented stages are `created`, `registering`, `booting`, `ready`, `running`, `stopping` and `stopped`. There is no `configuring` enum stage.

Application exposes `create`, `provider`, `boot`, `shutdown`, `run`, `listen`, `stop`, and lifecycle inspection. Hooks are registered through `on_boot`, `on_ready` and `on_shutdown`. Providers expose `register_services`, boot and shutdown hooks.

## Limits and planned work

Dependency-graph ordering, automatic ownership of every background service and full deadline-bounded draining are design requirements rather than universal guarantees. Configure readiness checks and runtime supervision explicitly.

## Implementation references

- [include/gungnir/core/application.hpp](../include/gungnir/core/application.hpp)
- [include/gungnir/core/lifecycle.hpp](../include/gungnir/core/lifecycle.hpp)
- [include/gungnir/core/provider.hpp](../include/gungnir/core/provider.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/application-lifecycle.md).
