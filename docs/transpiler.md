# Transpiler

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/transpiler.md).

## Current behavior

Transpiler tokenizes source, parses Program, runs semantic diagnostics, invokes specialized lowerers and applies SourceEdit replacements to source. It emits inspectable C++ with optional line directives.

## Limits and planned work

Lowerers still accept source/tokens. There is no separate complete C++ IR emitter consuming a dedicated Validated AST. Native compilation is required to catch remaining type/interface failures; --check only validates the current frontend/lowering diagnostics.

## Implementation references

- [src/language/transpiler.cpp](../src/language/transpiler.cpp)
- [include/gungnir/language/transpiler.hpp](../include/gungnir/language/transpiler.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/transpiler.md).
