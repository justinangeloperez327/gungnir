# Async Runtime

> **Status: experimental, pre-1.0.** Phase 11 defines the current ownership and cooperative-cancellation baseline. Native async APIs are not yet 1.0-stable.

## Current behavior

Native asynchronous execution uses `Task`, executors, cancellation tokens and timers. Middleware continuations and router handlers return `Task<Response>`. The language async lowerer emits native coroutine code from supported typed methods.

`Request::cancellation()` represents the request lifetime. The HTTP server cancels it when a client disconnects, a request exceeds its deadline, or shutdown reaches its drain deadline.

Long-lived response work can participate in the same lifetime:

- `BodyStream::CancellableProducer` receives the originating request token;
- `BodyStream::CancellableSyncProducer` receives the token before synchronous production;
- `WebSocketSession::CancellableHandler` receives the originating request token;
- `WebSocketSession::CancellableSyncHandler` receives the token before synchronous handling;
- `sleep_for(duration, token)` wakes through cancellation and throws `OperationCancelled`.

Legacy producer/handler overloads remain supported, but they cannot be interrupted by the runtime unless their own work observes cancellation through some other mechanism.

## Ownership rules

The HTTP runtime retains request state for asynchronous handlers, streaming responses and WebSocket sessions until their owned dispatch completes or is cancelled. The reactor tracks active detached dispatches and does not report the listen lifecycle stopped until those cooperative dispatches have settled.

`TaskGroup` owns a bounded set of cooperative child tasks, cancels siblings on failure, and joins before destruction; `join()` rethrows the first failure. Each child currently uses a thread. Executors bound pending work and expose background errors through `failure()` / `rethrow_failure()`.

Destroying an already suspended timer/executor task invalidates its queued continuation. Task owners must still avoid concurrently destroying a running coroutine.

## Limits

Cancellation is cooperative. A synchronous database, filesystem or user callback does not become non-blocking because its caller is a coroutine. Gungnir does not forcibly destroy a coroutine that is currently executing or suspended in an uncooperative external operation.

For request-lifetime async work, prefer token-aware operations and propagate the provided token through nested waits and child tasks.

## Implementation references

- [include/gungnir/core/task.hpp](../include/gungnir/core/task.hpp)
- [include/gungnir/core/executor.hpp](../include/gungnir/core/executor.hpp)
- [include/gungnir/core/cancellation.hpp](../include/gungnir/core/cancellation.hpp)
- [include/gungnir/core/timer.hpp](../include/gungnir/core/timer.hpp)
- [include/gungnir/http/stream.hpp](../include/gungnir/http/stream.hpp)
- [include/gungnir/http/websocket.hpp](../include/gungnir/http/websocket.hpp)
- [Runtime Correctness](runtime-correctness.md)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/async-runtime.md).
