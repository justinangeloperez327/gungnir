# Logging and Observability

> **Status: Gungnir 1.0 current contract.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/logging-observability.md).

## Current behavior

Logging, tracing and metrics have native APIs and memory sinks. Framework paths record HTTP/database/messaging-related instrumentation where integrated. An optional OTLP HTTP exporter is built with `GUNGNIR_WITH_OTLP` and exported as `gungnir::otlp`.

Configure sinks/exporters explicitly; bound buffering and exclude credentials and sensitive payloads.

## Limits and planned work

An exporter is not automatically configured by enabling its build flag. Do not promise complete distributed context propagation or telemetry coverage for every coroutine/native callback without validating that path.

## Implementation references

- [include/gungnir/logging/logger.hpp](../include/gungnir/logging/logger.hpp)
- [include/gungnir/observability/trace.hpp](../include/gungnir/observability/trace.hpp)
- [include/gungnir/observability/metrics.hpp](../include/gungnir/observability/metrics.hpp)
- [include/gungnir/observability/otlp_http_exporter.hpp](../include/gungnir/observability/otlp_http_exporter.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/logging-observability.md).
