# Testing

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/testing.md).

## Current behavior

Native `testing::Http` dispatches through the real router and calls the resulting task. It exposes `get`, `post` and generic `send`; use `send` for other HTTP methods. Response assertions and application helpers are provided.

Use memory stores/transports/sinks to inspect application behavior. Compile `.gnr` examples to generated C++ and check that generated code compiles with real framework headers.

## Limits and planned work

A successful `gungnirc --check` is not a full native type check or a live transport/integration test. Router-only tests do not cover TLS, HTTP parsing, database connectivity or distributed leases. Run relevant integration tests for the selected adapter.

## Implementation references

- [include/gungnir/testing/http.hpp](../include/gungnir/testing/http.hpp)
- [include/gungnir/testing/response_assertions.hpp](../include/gungnir/testing/response_assertions.hpp)
- [include/gungnir/testing/application.hpp](../include/gungnir/testing/application.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/testing.md).
