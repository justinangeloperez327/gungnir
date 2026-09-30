# Middleware

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/middleware.md).

The structured profile (`gungnirc --strict`) supports typed functions and framework actions, structured callbacks, and validated C++ emission. See [compiler profiles](compiler-profiles.md) for usage and current limits. The compatibility profile retains the native syntax described below.

## Current behavior

Native middleware handles `Request&` and `Next`, returning `Response` or `Task<Response>`. `Next` is a callable continuation returning `Task<Response>`.

Application registration supports middleware types, aliases, groups and priority. Middleware resolution uses request services when available. Use the generated `make:middleware` file as the current language starting point.

## Limits and planned work

The implicit-return `public async handle(...)` syntax is planned. A continuation is asynchronous even when other work in middleware is synchronous; forward its result according to the typed async contract.

## Implementation references

- [include/gungnir/http/middleware.hpp](../include/gungnir/http/middleware.hpp)
- [include/gungnir/http/middleware_registry.hpp](../include/gungnir/http/middleware_registry.hpp)
- [src/language/middleware_lowering.cpp](../src/language/middleware_lowering.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/middleware.md).
