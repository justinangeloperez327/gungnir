# HTTP Runtime

Gungnir's HTTP/1.1 socket backend uses a cross-platform non-blocking readiness reactor.

`RuntimeOptions` centralizes request/header limits, connection limits, request limits per persistent connection, read/write/idle timeouts, graceful-shutdown timeout, and keep-alive policy.

## Connection reactor

Listener and accepted sockets are non-blocking. The reactor waits for read/write readiness rather than assigning one blocking socket to a worker. It applies bounded connection admission, reads one request at a time per connection, buffers only within configured request limits, and writes responses incrementally when the socket is writable.

The reactor supports sequential HTTP/1.1 keep-alive requests and closes a connection when the client requests `close`, the server disables keep-alive, or `max_requests_per_connection` is reached. HTTP/1.0 remains close-by-default unless the client explicitly requests keep-alive.

Header and request limits are enforced before routing. Oversized headers return 431; oversized requests return 413.

Read, write and idle phases are bounded by their corresponding runtime timeouts.

## Shutdown

`Server::stop()` stops accepting new connections. Existing connections are allowed to finish within `shutdown_timeout`; idle connections are closed immediately and any remaining connections are forcibly released when the drain deadline expires.

## Bound port

`Server::bound_port()` exposes the actual listener port. This is useful for tests and for deployments that intentionally bind port `0`.

## Remaining transport work

Controller-facing Gungnir syntax does not expose C++ coroutine machinery, but the current HTTP dispatcher still completes route tasks inline. A handler that genuinely suspends is therefore not yet scheduled by the socket reactor. Integrating coroutine continuation scheduling with the executor is the next runtime step.

`Transport` remains the boundary for future TLS-backed transports. `BodyStream` remains the response-streaming foundation.

This runtime does not yet claim HTTP/2, TLS termination, WebSocket frame handling, chunked request parsing, or asynchronous streaming responses.
