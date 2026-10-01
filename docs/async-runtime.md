# Async Runtime

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/async-runtime.md).

## Current behavior

Native asynchronous execution uses Task, executors, cancellation tokens and timers. Middleware continuations and router handlers return `Task<Response>`. The language async lowerer emits native coroutine code from supported typed methods.

## Limits and planned work

Cancellation is cooperative. A synchronous database or storage call does not become non-blocking simply because its caller is a coroutine. `TaskGroup` owns a bounded set of cooperative child tasks, cancels siblings on failure, and joins before destruction; `join()` rethrows the first failure. Each child currently uses a thread. `sleep_for(duration, token)` responds to cancellation. Executors bound pending work and expose background errors through `failure()`/`rethrow_failure()`. Destroying an already suspended timer/executor task invalidates its queued continuation. Task owners must still avoid concurrently destroying a running coroutine; cancellation is the shutdown mechanism for owned child work.

## Implementation references

- [include/gungnir/core/task.hpp](../include/gungnir/core/task.hpp)
- [include/gungnir/core/executor.hpp](../include/gungnir/core/executor.hpp)
- [include/gungnir/core/cancellation.hpp](../include/gungnir/core/cancellation.hpp)
- [include/gungnir/core/timer.hpp](../include/gungnir/core/timer.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/async-runtime.md).
