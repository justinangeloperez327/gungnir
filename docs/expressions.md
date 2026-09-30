# Expressions

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/expressions.md).

## Current behavior

ExpressionKind includes name, literal, call, raw, unary, binary, member, subscript, group, object, entry and list. Expressions retain spans and child arguments.

## Limits and planned work

Raw expressions and text remain transitional escape paths. Arrow closures, named arguments, null-safe access and all target operator rules are not a complete implemented grammar.

## Implementation references

- [include/gungnir/language/ast.hpp](../include/gungnir/language/ast.hpp)
- [src/language/parser.cpp](../src/language/parser.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/expressions.md).
