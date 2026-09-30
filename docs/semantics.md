# Semantic Analysis

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/semantics.md).

## Current behavior

`ProgramValidator` performs structured module/name/type binding, member visibility, call/default/named argument checks, mutability, async/await checks, callback contracts, required framework methods and return-path checks. Errors preserve file, line, column and diagnostic codes; invalid programs cannot reach `CppEmitter`.

## Limits and planned work

Loop termination is not treated as proof of a return path. Native overloads and advanced null-flow analysis remain outside this profile. `SemanticAnalyzer` remains available for the compatibility frontend.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/language/semantic.hpp](../include/gungnir/language/semantic.hpp)
- [src/language/semantic.cpp](../src/language/semantic.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/semantics.md).
