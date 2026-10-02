# Stability

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/stability.md).

## Current behavior

The repository/CLI version is 0.1.0 and public APIs are experimental. Pin the exact commit or version tested by your application. Compatibility surfaces include `.gnr` syntax, generated native code, runtime APIs, adapters and compiler tooling.

Current usage guides live at this directory level; proposed contracts live under `design/`. Design requirements are not release guarantees.

The structured compiler is the canonical `gungnirc` profile. Legacy/native-compatible source requires explicit `--compat`; existing projects without `profile=structured` retain their legacy project build path during the pre-1.0 migration window.

## Limits and planned work

The compiler now reports `language_version = "0.1"` and experimental compatibility. Feature flags in spec.hpp describe implemented structured syntax rather than a complete stable grammar. Feature acceptance must be established by parsing, semantic checks and native compilation.

For compiler-facing features, the repository uses the [Compiler Correctness](compiler-correctness.md) invariants and [Compiler Conformance](compiler-conformance.md) matrix. Parser-only support is not considered a stable or complete language feature. A feature reaches compiler-complete status only after structural parsing, semantic validation, validated representation, code generation, negative diagnostics and representative native execution are covered.

## Implementation references

- [README.md](../README.md)
- [include/gungnir/language/spec.hpp](../include/gungnir/language/spec.hpp)
- [tools/gungnir.cpp](../tools/gungnir.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/stability.md).
