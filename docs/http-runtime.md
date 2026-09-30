# HTTP Runtime

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/http-runtime.md).

## Current behavior

Application exposes `http_runtime`, `tls`, `http2`, `listen`, `stop` and runtime inspection. HTTP/1.1 is the core protocol; optional TLS and HTTP/2 builds use `GUNGNIR_WITH_TLS` and `GUNGNIR_WITH_HTTP2`. CMake requires TLS when enabling HTTP/2.

RuntimeOptions includes request/header byte limits, connection count, requests per connection, stream/WebSocket limits, read/write/idle/request timeouts, stream/WebSocket timeouts, shutdown timeout and keep-alive. Defaults include a 1 MiB request limit, 64 KiB header limit, 4096 connections and 100 requests per persistent connection.

## Limits and planned work

Protocol types and ALPN entries alone do not prove complete protocol support. Optional transports require their dependencies and configuration. Validate backpressure and shutdown behavior for the deployed workload; do not advertise unlimited concurrency or guaranteed completion of all active work.

## Implementation references

- [include/gungnir/http/runtime.hpp](../include/gungnir/http/runtime.hpp)
- [include/gungnir/http/server.hpp](../include/gungnir/http/server.hpp)
- [include/gungnir/http/transport.hpp](../include/gungnir/http/transport.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/http-runtime.md).
