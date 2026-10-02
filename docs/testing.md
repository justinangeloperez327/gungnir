# Testing

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/testing.md).

## Current behavior

Native `testing::Http` dispatches through the real router and calls the resulting task. It exposes `get`, `post` and generic `send`; use `send` for other HTTP methods. Response assertions and application helpers are provided.

Use memory stores/transports/sinks to inspect application behavior. Compile `.gnr` examples to generated C++ and check that generated code compiles with real framework headers.

## Compiler correctness tests

Compiler testing is split by responsibility:

- **compile-pass** cases prove that valid structured programs parse, validate and emit;
- **compile-fail** cases prove that invalid programs stop before code generation and retain Gungnir source diagnostics;
- **run-pass/generated** cases compile emitted C++ against the real runtime and verify observable behavior;
- **regression** cases permanently cover fixed compiler defects;
- **robustness** cases feed malformed source and require controlled diagnostics rather than crashes;
- **determinism** cases require identical validated dumps and emitted output for identical inputs, including reordered multi-file input.

`tests/compiler_correctness.cpp` is the focused invariant gate for the structured compiler. It complements `structured_language.cpp`, `frontend_diagnostics.cpp` and generated/native execution tests.

See [Compiler Correctness](compiler-correctness.md) and [Compiler Conformance](compiler-conformance.md).

## Limits and planned work

A successful `gungnirc --check` is not a full native type check or a live transport/integration test. Router-only tests do not cover TLS, HTTP parsing, database connectivity or distributed leases. Run relevant integration tests for the selected adapter.

## Implementation references

- [include/gungnir/testing/http.hpp](../include/gungnir/testing/http.hpp)
- [include/gungnir/testing/response_assertions.hpp](../include/gungnir/testing/response_assertions.hpp)
- [include/gungnir/testing/application.hpp](../include/gungnir/testing/application.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/testing.md).
