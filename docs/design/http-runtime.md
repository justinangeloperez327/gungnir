# HTTP Runtime

> **Gungnir 1.x design specification.** This document describes intended long-term architecture beyond the frozen `1.0.0-rc.1` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../http-runtime.md) before using an API.


## 1.0 RC alignment

The shipped baseline is `1.0.0-rc.1`. The 1.0 contracts are frozen; this design document may describe additive 1.x evolution or future-major work, but current implementation guides and executable tests remain authoritative.

The HTTP runtime is the transport layer beneath Gungnir routing, Request, Response, middleware, sessions, authentication, and controller execution.

Application-facing HTTP contracts are defined in routing.md, request.md, response.md, and middleware.md.

# Responsibilities

The HTTP runtime owns:

- listener sockets;
- connection admission;
- HTTP parsing;
- limits and timeouts;
- request construction;
- router dispatch;
- async request ownership;
- response serialization;
- keep-alive;
- graceful shutdown;
- transport-level cancellation.

Application controllers do not manage sockets.

# HTTP/1.1

The built-in HTTP/1.1 backend uses non-blocking readiness-oriented connection handling.

It should support sequential keep-alive requests per connection while respecting configured limits.

A connection closes when required by protocol/client request, server policy, maximum requests per connection, malformed input, timeout, or shutdown.

# Limits

RuntimeOptions should centralize limits such as:

~~~text
header bytes
request body bytes
connection count
requests per persistent connection
read timeout
write timeout
idle timeout
request timeout
shutdown timeout
~~~

These are security and reliability controls.

# Parse failures

Transport/parser errors should produce appropriate HTTP responses where possible.

Examples:

~~~text
oversized headers -> 431
oversized body    -> 413
malformed request -> 400
~~~

Connection safety may require closing after malformed input.

# Request ownership

Once a request is dispatched, its state must remain valid until the controller/middleware task completes or is cancelled.

This includes async suspension.

Request data must not reference receive buffers that become invalid while the coroutine is suspended.

# Async dispatch

A route task may suspend.

The runtime retains connection/request state and resumes response processing when the task completes.

Coroutine continuations may run on executor threads while socket readiness remains reactor-owned.

# Backpressure

The runtime should not buffer unbounded request or response data.

Streaming and large transfers should use bounded buffering and write readiness.

# Keep-alive

HTTP/1.1 keep-alive follows protocol and runtime policy.

The runtime must serialize request/response processing correctly for a connection unless pipelining or multiplexing is explicitly supported.

# Graceful shutdown

Server shutdown should:

1. stop accepting new connections;
2. stop accepting new keep-alive work where required;
3. let active dispatched requests drain within deadline;
4. signal cancellation after deadline;
5. close remaining sockets and resources.

# Bound port

The server should expose the actual bound port.

This is useful for tests and port-zero development binding.

# TLS

TLS may be implemented by the built-in runtime or an external reverse proxy.

When built in, TLS must integrate with the same non-blocking lifecycle, timeouts, shutdown, and certificate configuration.

# HTTP/2 and WebSocket

Protocol support must be documented according to actual repository capability.

A feature should not be advertised solely because a parser or type exists.

Protocol-specific backpressure, cancellation, and shutdown semantics must remain consistent with the application request model.

# Trusted proxy boundary

Client scheme, IP, and host forwarding metadata must only be accepted from configured trusted peers.

Transport parsing alone does not make forwarded headers trustworthy.

# Request IDs and tracing

The HTTP boundary may create or accept request identifiers according to configured security policy and attach them to observability context.

Tracing context must survive async execution.

# Runtime/application boundary

Transport parsing produces a Request.

Router, middleware, and controller execution produce a Response.

The HTTP runtime serializes that Response.

Application code should not manipulate native socket buffers.

# Design rule

~~~text
HTTP runtime owns transport
router owns matching
middleware owns request pipeline policy
controller owns application action
Response owns application response intent
~~~

