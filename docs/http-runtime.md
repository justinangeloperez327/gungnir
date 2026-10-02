# HTTP Runtime

> **Status: experimental, pre-1.0.** Phase 11 establishes the current runtime-lifecycle correctness baseline. Native C++ APIs remain subject to pre-1.0 compatibility rules.

## Current behavior

`Application` exposes `http_runtime`, `tls`, `http2`, `listen`, `stop` and runtime inspection. HTTP/1.1 is the core protocol; optional TLS and HTTP/2 builds use `GUNGNIR_WITH_TLS` and `GUNGNIR_WITH_HTTP2`. CMake requires TLS when enabling HTTP/2.

`RuntimeOptions` includes request/header byte limits, connection count, requests per connection, stream/WebSocket limits, read/write/idle/request timeouts, stream/WebSocket timeouts, shutdown timeout and keep-alive. Defaults include a 1 MiB request limit, 64 KiB header limit, 4096 connections and 100 requests per persistent connection.

## Runtime lifecycle

The server distinguishes **active** from **accepting**:

- `Server::running()` and `Application::is_running()` remain true for the complete listen lifecycle, including graceful drain and cooperative dispatch cleanup;
- `Server::accepting()` and `Application::is_accepting()` become false as soon as `stop()` starts shutdown;
- the listener is closed when drain begins, so no new connections are admitted;
- a second `listen()` call is rejected while the previous server lifecycle is still draining;
- `active_dispatches()` / `active_http_dispatches()` expose currently owned asynchronous request, stream and WebSocket dispatch work.

This distinction is required by `RuntimeHost` supervision: the HTTP runtime is not considered stopped merely because it stopped accepting new requests.

## Timeouts and cancellation

Each parsed request owns a cancellation source. Disconnects, request timeouts and forced shutdown cancel that request token.

A streaming response retains the originating request lifetime. Cancellation-aware `BodyStream` producers receive the request token, so stream-chunk timeout, disconnect and shutdown can interrupt cooperative asynchronous producers.

A WebSocket upgrade also retains the originating request lifetime. Cancellation-aware WebSocket handlers receive that token, so message timeout, disconnect and shutdown can interrupt cooperative asynchronous handlers.

Existing non-cancellable `BodyStream::Producer` and `WebSocketSession::Handler` overloads remain available for source compatibility. Long-lived asynchronous work should use the cancellable forms.

## Graceful shutdown

`stop()` begins graceful drain:

1. stop accepting new connections;
2. preserve already in-flight HTTP work long enough to finish normally;
3. close idle connections and mark active HTTP/1 responses to close after their final write;
4. initiate protocol-appropriate shutdown for HTTP/2 and WebSocket connections;
5. at `shutdown_timeout`, cancel retained request lifetimes and close remaining connections;
6. wait for owned cooperative dispatches to finish before the listen lifecycle reports stopped.

`shutdown_timeout` is a **drain deadline**, not a mechanism for forcibly destroying arbitrary C++ code.

## Limits

Cancellation is cooperative. A request handler, stream producer or WebSocket handler that performs blocking work or ignores its cancellation token can delay final dispatch cleanup after the network drain deadline. Gungnir does not destroy a running coroutine from another thread because doing so would be unsafe.

Protocol types and ALPN entries alone do not prove complete protocol support. Optional transports still require their dependencies and configuration. Validate workload-specific backpressure and resource limits before production deployment.

## Implementation references

- [include/gungnir/http/runtime.hpp](../include/gungnir/http/runtime.hpp)
- [include/gungnir/http/server.hpp](../include/gungnir/http/server.hpp)
- [include/gungnir/http/stream.hpp](../include/gungnir/http/stream.hpp)
- [include/gungnir/http/websocket.hpp](../include/gungnir/http/websocket.hpp)
- [src/http/server.cpp](../src/http/server.cpp)
- [Runtime Correctness](runtime-correctness.md)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/http-runtime.md).
