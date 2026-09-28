# Production and Deployment

Gungnir exposes a small health/readiness contract while keeping deployment policy outside the framework.

## Health

`production::Health::live()` represents process liveness. Readiness checks are explicit callbacks registered by the application. `ready()` succeeds only when every registered readiness check succeeds.

Readiness checks should cover dependencies that must be available before an instance receives traffic. Expensive diagnostics should not be placed on high-frequency health endpoints.

## Shutdown

The HTTP server performs cooperative graceful draining when `stop()` is requested. It closes the listener first, refuses additional keep-alive work, and allows requests that were already dispatched to finish and flush their response with `Connection: close`.

`RuntimeOptions::shutdown_timeout` is the drain deadline. When the deadline expires, remaining request cancellation tokens are signaled and their client sockets are closed. Request handlers that perform cancellable work should observe the request cancellation token so shutdown can complete promptly after the deadline.

HTTP, queue workers and the scheduler each expose cooperative shutdown primitives. Queue workers finish the active job before exiting; the scheduler finishes the active synchronous task and refuses to start another task after stop/cancellation is observed.

Gungnir provides `production::Supervisor` as the process-wide shutdown coordination primitive. A supervisor owns one shared cancellation source, fans stop requests out to registered runtimes, and waits for their running checks to drain within one overall shutdown deadline.

Runtime registration is explicit and named. Stop requests are idempotent, callback failures are isolated, and a timed-out shutdown reports the runtimes that remain active rather than silently claiming a graceful exit.

Typed adapters are available through `production::supervise(...)` for `Application`, `queue::Worker`, and `scheduler::Scheduler`. The adapters bind each runtime's native stop/running contract to the supervisor so hosting code does not need to duplicate shutdown lambdas. Queue workers and schedulers can run with `supervisor.token()`, giving them both shared cancellation and their native stop signal.

`production::RuntimeHost` provides application-owned service startup and join policy on top of the supervisor. Queue workers and schedulers are registered before `start()`, then run on dedicated service threads with the supervisor's shared cancellation token. The HTTP listener also registers that same token after establishing its running state, so cancellation that happens before or during listener startup cannot be lost. An already-cancelled process exits HTTP startup before binding a socket. An unhandled background runtime exception is captured and triggers process-wide stop fan-out; callers can inspect or rethrow it through the host.

`RuntimeHost::shutdown()` requests one coordinated shutdown and waits only until the supervisor's top-level deadline. Managed service threads are joined automatically after all runtimes report drained. If the deadline expires, the result reports pending runtimes and the call returns without blocking past the deadline. Gungnir does not attempt unsafe in-process thread termination; an external service manager may terminate the process according to deployment policy after a reported timeout.

`RuntimeHost::run(port, host)` starts managed background services, runs the application's HTTP listener on the calling thread, coordinates shutdown when the listener returns, and rethrows any captured background runtime failure after cleanup.

## Reverse proxies

Applications deployed behind a reverse proxy must configure trust at the HTTP boundary. Forwarded headers must not be trusted merely because they are present. Proxy address ranges, forwarded host/protocol handling and client IP resolution require an explicit trust policy.

## Configuration

Production configuration should be supplied through the application's environment/configuration facilities. Secrets must not be committed to generated project files or emitted through diagnostics, HTTP error pages or logs.

## HTTPS / TLS

Gungnir can terminate HTTPS directly when built with `GUNGNIR_WITH_TLS=ON`. TLS support uses OpenSSL 3 and remains optional so the default core build has no OpenSSL dependency.

Configure HTTPS with `Application::tls(http::TlsOptions{...})` or by setting `RuntimeOptions::tls`. A certificate chain and private key are required. Encrypted connections use the same nonblocking reactor as plaintext HTTP: TLS handshakes, reads, writes, request limits, timeouts, keep-alive behavior, graceful process draining and connection metrics all share the existing server lifecycle.

The server requires TLS 1.2 or newer, disables TLS-level compression, supports password-protected PEM private keys, and can optionally require a client certificate backed by a configured CA bundle. ALPN is explicit. When Gungnir is built with `GUNGNIR_WITH_HTTP2=ON` and `Application::http2(...)` is configured, the TLS endpoint advertises `h2` ahead of `http/1.1`; otherwise it advertises only enabled protocols.

Normal HTTP connection closure attempts a nonblocking TLS `close_notify`. Error, cancellation and timeout paths close immediately so a faulty peer cannot extend the framework's shutdown deadline.

## Streaming and backpressure

`http::BodyStream` is a coroutine-aware response producer. `Response::stream(...)` sends HTTP/1.1 chunked responses without materializing the complete body.

The reactor enforces one-chunk-at-a-time backpressure. It does not request the next producer chunk until the previous encoded chunk has drained from the connection's output buffer. `RuntimeOptions::max_stream_chunk_bytes` bounds an individual producer result (64 KiB by default), and `stream_chunk_timeout` bounds how long one asynchronous producer step may remain pending.

Stream producers may suspend with normal Gungnir coroutine primitives such as `sleep_for`. Producer continuations are tracked by the HTTP dispatch tracker so shutdown does not silently abandon framework-owned stream work. Disconnects and server shutdown cancel the retained request context so a producer that observes the request cancellation token can stop promptly.

The server owns response framing. User-provided `Content-Length`, `Transfer-Encoding`, and `Connection` headers cannot override the framing chosen by the runtime. Streaming responses use `Transfer-Encoding: chunked`; ordinary responses use framework-generated `Content-Length`. Keep-alive remains valid after a completed stream, including pipelined input already buffered on the connection.

## Current runtime limitation

The core server provides production HTTPS/TLS termination, HTTP/1.1 coroutine response streaming with bounded write-side backpressure, WebSocket upgrade/data-plane handling, and optional HTTP/2 over TLS/ALPN using nghttp2 for framing and HPACK. HTTP/2 request streams are multiplexed through the same router, middleware, IoC, session/auth, tracing and cancellation path as HTTP/1.1. `Http2Options` bounds concurrent streams and header-list size; the existing request-size and request-timeout limits remain enforced per stream. Graceful shutdown sends GOAWAY, refuses new streams, lets already-dispatched streams finish until the process deadline, then cancels remaining request tokens.

HTTP/2 currently supports buffered request bodies and ordinary buffered `Response` bodies. `Response::stream(...)` and WebSocket upgrades on an HTTP/2 stream return 501 rather than silently falling back; HTTP/2 streaming DATA providers and RFC 8441 extended CONNECT are explicit follow-up work. Incoming request-body streaming is also not yet implemented. Deployments should validate connection limits, stream concurrency and throughput under representative load.

## Deployment responsibility

Container images, service managers, TLS termination, log shipping, metrics exporters, process supervision and zero-downtime orchestration are deployment concerns. Gungnir should provide integration points without pretending to replace those systems.


## Process signals

`production::SignalWatcher` provides an opt-in bridge from `SIGINT` and `SIGTERM` into normal Gungnir shutdown code.

The asynchronous signal handler itself performs only one signal-safe operation: it stores the signal number in a `sig_atomic_t`. A dedicated watcher thread observes that value and invokes the configured callback from normal thread context. Framework mutexes, allocations, logging, cancellation and runtime stop methods are therefore never called directly from asynchronous signal context.

Only one Gungnir signal watcher may own the process handlers at a time. The watcher restores the handlers that were installed before it was created when it stops or is destroyed. Ownership is tracked so an older, already-stopped watcher cannot restore handlers belonging to a newer watcher.

`RuntimeHost::run_with_signals()` installs a watcher that maps `SIGINT` / `SIGTERM` to `RuntimeHost::request_stop()`, then runs the normal application/runtime lifecycle. This provides a convenient service-manager entry point while keeping signal handling opt-in.

A second signal does not forcibly terminate worker threads or request handlers. If cooperative shutdown exceeds the configured process deadline, `ShutdownResult` identifies the still-running runtimes and the deployment/service manager remains responsible for any hard process termination policy.
