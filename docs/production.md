# Production and Deployment

> **Status: experimental, pre-1.0.** Phase 11 hardens process/runtime shutdown semantics, but this is not a blanket production-certification claim.

## Current behavior

Health supports named readiness callbacks. `Health::live()` currently returns true; `ready()` evaluates registered callbacks and returns false when one returns false. `RuntimeHost`, runtime adapters, signal watching and supervision provide explicit process-runtime integration.

`RuntimeHost` supervises the HTTP application, queue workers and scheduler through a shared cancellation boundary. HTTP liveness now remains true during graceful drain, so the supervisor cannot report a successful HTTP stop merely because the listener stopped accepting connections.

A graceful HTTP stop closes admission first, drains active work, applies the configured shutdown deadline, cancels retained request lifetimes when the deadline expires, and waits for cooperative dispatch cleanup.

Register required dependency checks, configure HTTP limits/timeouts, choose TLS or a reverse proxy, and run workers/scheduler under an appropriate host.

## Shutdown contract

`Supervisor::shutdown()` is bounded by its configured supervisor timeout and returns the names of runtimes that still report running. For HTTP, `Application::is_running()` now covers the complete active/draining listen lifecycle rather than only listener admission.

The HTTP server's own `shutdown_timeout` bounds network draining. These are distinct controls:

- HTTP `shutdown_timeout`: when remaining connections/request lifetimes are cancelled and closed;
- supervisor `shutdown_timeout`: how long process supervision waits for registered runtimes to report stopped.

Cancellation remains cooperative. User code that blocks or ignores cancellation can delay final thread/coroutine cleanup. Gungnir does not use unsafe forced thread or coroutine termination.

## Limits

The Health object does not install HTTP endpoints automatically; `live()` is not an external process probe. Readiness callbacks can throw. Runtime lifecycle hardening does not remove the need for workload-specific load testing, dependency health checks, deployment probes and external process supervision.

## Implementation references

- [include/gungnir/production/health.hpp](../include/gungnir/production/health.hpp)
- [include/gungnir/production/runtime_host.hpp](../include/gungnir/production/runtime_host.hpp)
- [include/gungnir/production/supervisor.hpp](../include/gungnir/production/supervisor.hpp)
- [include/gungnir/http/runtime.hpp](../include/gungnir/http/runtime.hpp)
- [Runtime Correctness](runtime-correctness.md)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/production.md).
