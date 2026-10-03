# Application Lifecycle

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

## Current behavior

The implemented stages are `created`, `registering`, `booting`, `ready`, `running`, `stopping` and `stopped`. There is no `configuring` enum stage.

Application exposes `create`, `provider`, `boot`, `shutdown`, `run`, `listen`, `stop`, and lifecycle inspection. Hooks are registered through `on_boot`, `on_ready` and `on_shutdown`. Providers expose `register_services`, `boot`, `ready` and `shutdown` hooks. Startup reaches `ready` only after every ready hook succeeds. A startup failure shuts down registered providers in reverse order, including a partially registered provider, then rethrows the original error. Shutdown attempts every cleanup hook and exposes collected exceptions through `shutdown_errors()`. A stopped or failed application is not reusable; construct a new instance.

Applications own their routing, container, database and view contexts. Use `app.activate()` around native work; request dispatch and framework task/executor paths carry their owning context. HTTP resources are initialized when listening starts, so constructing an application for a CLI task does not start an HTTP runtime.

## Limits and planned work

Dependency-graph ordering, automatic ownership of every background service and full deadline-bounded draining are design requirements rather than universal guarantees. Configure readiness checks and runtime supervision explicitly.

## Implementation references

- [include/gungnir/core/application.hpp](../include/gungnir/core/application.hpp)
- [include/gungnir/core/lifecycle.hpp](../include/gungnir/core/lifecycle.hpp)
- [include/gungnir/core/provider.hpp](../include/gungnir/core/provider.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/application-lifecycle.md).
