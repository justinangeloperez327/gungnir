# HTTP Runtime

Group 6 defines the transport-facing HTTP runtime independently from routing and controller semantics.

`RuntimeOptions` centralizes request/header limits, connection limits, request limits per persistent connection, read/write/idle timeouts, and keep-alive policy.

HTTP/1.1 persistent connection policy is represented explicitly rather than hard-coded into response serialization. The wire layer can emit either `keep-alive` or `close`.

`Transport` is the boundary for plain TCP and future TLS-backed transports. TLS is intentionally represented as a boundary rather than embedding a specific TLS library into framework APIs.

`BodyStream` establishes producer-based streaming semantics for future response streaming and chunked transfer encoding.

WebSocket upgrade detection is represented separately from ordinary HTTP routing so a later handshake/frame implementation can reuse the same server connection lifecycle.

The current socket backend remains blocking. These APIs are foundations for moving connection readiness onto the Group 5 asynchronous runtime; they do not claim HTTP/2, TLS, WebSocket frames, chunked parsing, or a non-blocking socket backend is complete.
