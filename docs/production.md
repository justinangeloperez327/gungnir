# Production and Deployment

## Overview
Health supports named liveness and readiness callbacks with fail-closed evaluation. A callback that returns false or throws marks the probe unhealthy without propagating the dependency failure through the probe call. Named reports expose each check result.

`RuntimeHost` supervises the HTTP application, queue workers and scheduler through a shared cancellation boundary and exposes lifecycle-aware health. Its readiness becomes false during shutdown or after a fatal supervised-service failure.

A graceful HTTP stop closes admission first, drains active work, applies the configured shutdown deadline, cancels retained request lifetimes when the deadline expires, and waits for cooperative dispatch cleanup.

HTTP production limits now include `max_active_dispatches` in addition to connection, request-size and timeout bounds. New work above the active dispatch budget is shed with 503 rather than admitted without bound. `production::retry` provides explicit bounded exponential backoff for operations the application classifies as transient.

Register required dependency checks, configure HTTP limits/timeouts, choose TLS or a reverse proxy, and run workers/scheduler under an appropriate host. See [Production Resilience](production-resilience.md) for the Gungnir contract.

## Shutdown contract

Generated HTTP and background executables handle signals through native
cancellation. The HTTP executable drains the listener and calls application
shutdown, including configured tracing and metric exporter cleanup. Native
applications can pass a `CancellationToken` to `Application::run` or
`Application::listen` when using their own process supervision.

`Supervisor::shutdown()` is bounded by its configured supervisor timeout and returns the names of runtimes that still report running. For HTTP, `Application::is_running()` now covers the complete active/draining listen lifecycle rather than only listener admission.

The HTTP server's own `shutdown_timeout` bounds network draining. These are distinct controls:

- HTTP `shutdown_timeout`: when remaining connections/request lifetimes are cancelled and closed;
- supervisor `shutdown_timeout`: how long process supervision waits for registered runtimes to report stopped.

Cancellation remains cooperative. User code that blocks or ignores cancellation can delay final thread/coroutine cleanup. Gungnir does not use unsafe forced thread or coroutine termination.

## Limits

The Health object does not install HTTP endpoints automatically; `live()` is not an external process probe. Probe callbacks are isolated and fail closed, but applications still decide which dependencies belong in liveness versus readiness. Runtime lifecycle hardening does not remove the need for workload-specific load testing, dependency health checks, deployment probes and external process supervision.



- [include/gungnir/production/health.hpp](../include/gungnir/production/health.hpp)
- [include/gungnir/production/runtime_host.hpp](../include/gungnir/production/runtime_host.hpp)
- [include/gungnir/production/supervisor.hpp](../include/gungnir/production/supervisor.hpp)
- [include/gungnir/http/runtime.hpp](../include/gungnir/http/runtime.hpp)
- [Runtime Correctness](runtime-correctness.md)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/production.md).
