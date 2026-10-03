# Expressions

## Overview
The structured frontend parses typed literals, calls, members, indexing, lists, objects, arrows, unary/binary operators, ternaries, `await`, safe field access and null coalescing. Arrows receive callback parameter types from migration/collection contexts and capture referenced outer values by value. Optional values can be narrowed by simple null comparisons in if/else branches.

## Scope
String interpolation, general native operators, safe optional method calls and general flow-sensitive narrowing are not supported. Await expressions inside null coalescing require a separate binding. Native APIs must be declared through compiler bindings. Framework compatibility expressions continue through the legacy parser.



- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/language/ast.hpp](../include/gungnir/language/ast.hpp)
- [src/language/parser.cpp](../src/language/parser.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/expressions.md).
