# Runtime Correctness

> **Status: Gungnir 1.0 runtime-correctness contract.** This contract describes ownership, cancellation and graceful-shutdown behavior that the runtime test suite must preserve.

## Core invariants

### Server lifecycle

The HTTP server has separate admission and lifecycle states.

```text
inactive
   |
 listen()
   v
accepting
   |
 stop() / cancellation
   v
draining
   |
 connections closed + cooperative dispatches settled
   v
inactive
```

While draining:

- no new connections are accepted;
- `running()` remains true;
- `accepting()` is false;
- a second `listen()` call is rejected;
- active request/stream/WebSocket dispatches remain owned by the server.

The server becomes inactive only after connection cleanup and cooperative dispatch settlement.

### Request lifetime

Every parsed request owns one cancellation source. The token returned by `Request::cancellation()` is the authoritative request-lifetime token.

The runtime cancels that source on:

- client disconnect;
- request deadline;
- forced connection close;
- shutdown deadline;
- relevant stream/WebSocket lifetime termination.

Streaming responses and WebSocket upgrades retain the originating request state so cancellable producers/handlers receive the same lifetime boundary rather than creating unrelated cancellation domains.

### Graceful shutdown

A normal stop does not immediately cancel an in-flight HTTP handler. It stops admission and gives active work the configured drain window.

If the drain deadline is reached, the runtime cancels retained request lifetimes and closes remaining network connections. It then waits for runtime-owned cooperative dispatches to settle before reporting the listen lifecycle stopped.

### Resource ownership

The server tracks detached asynchronous dispatches explicitly. Closing a socket does not imply its application work has completed. A runtime is considered fully stopped only after the tracked dispatch count reaches zero.

## Cooperative-cancellation boundary

Gungnir cancellation is cooperative. Token-aware waits such as `sleep_for(duration, token)` can stop promptly. Blocking C++ calls or handlers that ignore cancellation cannot be safely killed by the runtime.

Therefore:

- `shutdown_timeout` bounds the network drain phase;
- it does not promise forced termination of arbitrary user code;
- long-lived request work should use token-aware operations;
- process supervisors should still enforce an external deployment-level termination policy when required.

## Regression gate

Primary CI builds and runs:

```text
gungnir.runtime_safety
gungnir.timer_runtime
gungnir.http_reactor
gungnir.http_server
gungnir.http_streaming
gungnir.websocket
gungnir.runtime_supervisor
gungnir.runtime_host
gungnir.runtime_lifecycle
```

The dedicated lifecycle regression verifies both graceful completion before the deadline and cooperative cancellation at the deadline.

## Scope

Phase 11 is about runtime correctness, not performance tuning or framework-feature expansion. Database/ORM behavioral correctness is handled separately in Phase 12.
