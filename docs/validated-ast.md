# Validated AST

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/validated-ast.md).

## Current behavior

There is no dedicated Validated AST layer today. Parser output is checked by SemanticAnalyzer and passed to specialized lowerers.

## Limits and planned work

The target fully resolved typed handoff is a design specification. Do not describe current code generation as consuming ValidatedProgram or guaranteed SymbolId/TypeId bindings.

## Implementation references

- [include/gungnir/language/ast.hpp](../include/gungnir/language/ast.hpp)
- [include/gungnir/language/semantic.hpp](../include/gungnir/language/semantic.hpp)
- [src/language/transpiler.cpp](../src/language/transpiler.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/validated-ast.md).
