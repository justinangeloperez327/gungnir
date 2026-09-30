# Production and Deployment

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/production.md).

## Current behavior

Health supports named readiness callbacks. `Health::live()` currently returns true; `ready()` evaluates registered callbacks and returns false when one returns false. RuntimeHost, runtime adapters, signal watching and supervision provide explicit process-runtime integration.

Register required dependency checks, configure HTTP limits/timeouts, choose TLS or a reverse proxy, and run workers/scheduler under an appropriate host. Pin and validate the framework revision.

## Limits and planned work

The Health object does not install HTTP endpoints automatically; `live()` is not an external process probe. Readiness callbacks can throw. Do not equate runtime primitives with production certification or full graceful draining guarantees.

## Implementation references

- [include/gungnir/production/health.hpp](../include/gungnir/production/health.hpp)
- [include/gungnir/production/runtime_host.hpp](../include/gungnir/production/runtime_host.hpp)
- [include/gungnir/production/supervisor.hpp](../include/gungnir/production/supervisor.hpp)
- [include/gungnir/http/runtime.hpp](../include/gungnir/http/runtime.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/production.md).
