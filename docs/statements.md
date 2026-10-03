# Statements

> **Status: Gungnir 1.0 current contract.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/statements.md).

## Current behavior

MethodStatement represents returns, bindings, expression statements, blocks, conditionals, loops, break and continue. Semantics checks selected scope, mutability, condition, return and loop-control rules.

## Limits and planned work

The target grammar is stricter and more complete than current parsing. Full definite assignment and all-path return analysis are not guaranteed for arbitrary native escape forms.

## Implementation references

- [include/gungnir/language/ast.hpp](../include/gungnir/language/ast.hpp)
- [src/language/parser.cpp](../src/language/parser.cpp)
- [src/language/semantic.cpp](../src/language/semantic.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/statements.md).
