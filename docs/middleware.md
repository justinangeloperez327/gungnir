# Middleware

> **Status: Gungnir 1.0 current contract.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/middleware.md).

The structured profile (`gungnirc --strict`) supports typed functions and framework actions, structured callbacks, and validated C++ emission. See [compiler profiles](compiler-profiles.md) for usage and current limits. The compatibility profile retains the native syntax described below.

## Current behavior

Native middleware handles `Request&` and `Next`, returning `Response` or `Task<Response>`. In the structured profile, middleware requires a public `handle(Request request, Next next)` method with logical result `Response`; `async handle(...)` lowers to `Task<Response>`. `Next` is a callable continuation returning `Task<Response>`.

Application registration supports middleware types, aliases, groups and priority. Middleware resolution uses request services when available. Use the generated `make:middleware` file as the current language starting point.

## Limits and planned work

The implicit-result middleware contract is implemented. Because `Next` is asynchronous, middleware that forwards the continuation normally uses `async handle(...)` and `await next(request)`. Invalid middleware signatures are rejected by `gungnirc --check` with `GNR2301`.

## Implementation references

- [include/gungnir/http/middleware.hpp](../include/gungnir/http/middleware.hpp)
- [include/gungnir/http/middleware_registry.hpp](../include/gungnir/http/middleware_registry.hpp)
- [src/language/middleware_lowering.cpp](../src/language/middleware_lowering.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/middleware.md).
