# Semantic Analysis

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/semantics.md).

## Current behavior

SemanticIndex records framework types, actions, declaration sources and method signatures. SemanticAnalyzer checks duplicate members/parameters, selected relationships/routes, known expression types, mutability, conditions and return/control-flow rules. Closed-world project mode enables additional unresolved-reference checks.

## Limits and planned work

Unknown native types/calls remain permissive. Complete symbol/type resolution, cross-module import visibility, framework capability checks and exhaustive control flow are not yet implemented. SemanticAnalyzer returns diagnostics rather than a fully typed validated program.

## Implementation references

- [include/gungnir/language/semantic.hpp](../include/gungnir/language/semantic.hpp)
- [src/language/semantic.cpp](../src/language/semantic.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/semantics.md).
