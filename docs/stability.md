# Stability

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/stability.md).

## Current behavior

The repository/CLI version is 0.1.0 and public APIs are experimental. Pin the exact commit or version tested by your application. Compatibility surfaces include `.gnr` syntax, generated native code, runtime APIs, adapters and compiler tooling.

Current usage guides live at this directory level; proposed contracts live under `design/`. Design requirements are not release guarantees.

## Limits and planned work

The compiler now reports `language_version = "0.1"` and experimental compatibility. Feature flags in spec.hpp describe implemented structured syntax rather than a complete stable grammar. Feature acceptance must be established by parsing, semantic checks and native compilation.

## Implementation references

- [README.md](../README.md)
- [include/gungnir/language/spec.hpp](../include/gungnir/language/spec.hpp)
- [tools/gungnir.cpp](../tools/gungnir.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/stability.md).
