# Async and Await

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

## Current behavior

The async lowerer handles supported explicitly typed methods and rewrites async/await into native coroutine forms. Native runtime uses Task<T>.

## Limits and planned work

Async is not evidence of non-blocking I/O. The target implicit framework return contracts, typed arrow async closures, structured concurrency and fully validated awaitability remain compiler/runtime work.

## Implementation references

- [src/language/async_lowering.cpp](../src/language/async_lowering.cpp)
- [include/gungnir/core/task.hpp](../include/gungnir/core/task.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/async.md).
