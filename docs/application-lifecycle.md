# Application Lifecycle

## Overview
The implemented stages are `created`, `registering`, `booting`, `ready`, `running`, `stopping` and `stopped`. There is no `configuring` enum stage.

Application exposes `create`, `provider`, `boot`, `shutdown`, `run`, `listen`, `stop`, and lifecycle inspection. Hooks are registered through `on_boot`, `on_ready` and `on_shutdown`. Providers expose `register_services`, `boot`, `ready` and `shutdown` hooks. Startup reaches `ready` only after every ready hook succeeds. A startup failure shuts down registered providers in reverse order, including a partially registered provider, then rethrows the original error. Shutdown attempts every cleanup hook and exposes collected exceptions through `shutdown_errors()`. A stopped or failed application is not reusable; construct a new instance.

Applications own their routing, container, database and view contexts. Use `app.activate()` around native work; request dispatch and framework task/executor paths carry their owning context. HTTP resources are initialized when listening starts, so constructing an application for a CLI task does not start an HTTP runtime.

## Running an application

Register providers and lifecycle hooks before booting the application. Configure readiness checks and process supervision explicitly when deploying multiple services.



- [include/gungnir/core/application.hpp](../include/gungnir/core/application.hpp)
- [include/gungnir/core/lifecycle.hpp](../include/gungnir/core/lifecycle.hpp)
- [include/gungnir/core/provider.hpp](../include/gungnir/core/provider.hpp)

See [Documentation](README.md) and [Getting Started](getting-started.md).
