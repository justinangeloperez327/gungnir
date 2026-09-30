# Async Runtime

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/async-runtime.md).

## Current behavior

Native asynchronous execution uses Task, executors, cancellation tokens and timers. Middleware continuations and router handlers return `Task<Response>`. The language async lowerer emits native coroutine code from supported typed methods.

## Limits and planned work

Cancellation is cooperative. A synchronous database or storage call does not become non-blocking simply because its caller is a coroutine. Structured task groups, complete semantic await typing and blanket context/lifetime guarantees in the target design remain work to verify per execution path.

## Implementation references

- [include/gungnir/core/task.hpp](../include/gungnir/core/task.hpp)
- [include/gungnir/core/executor.hpp](../include/gungnir/core/executor.hpp)
- [include/gungnir/core/cancellation.hpp](../include/gungnir/core/cancellation.hpp)
- [include/gungnir/core/timer.hpp](../include/gungnir/core/timer.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/async-runtime.md).
