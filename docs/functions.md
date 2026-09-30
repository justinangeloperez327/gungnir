# Functions

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/functions.md).

## Current behavior

Use `function int add(int left, int right = 2) { return left + right; }` with `gungnirc --strict`. Functions require declared return types. Calls support literal defaults and named arguments; supplied arguments are evaluated in source order. Framework methods infer their documented response/decision/void contracts. Public, protected and private members are checked.

## Limits and planned work

Defaults currently require literal constants. Native overload resolution, variadic functions and general class inheritance are outside this profile. Async calls require explicit `await` inside an async callable.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/language/ast.hpp](../include/gungnir/language/ast.hpp)
- [src/language/parser.cpp](../src/language/parser.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/functions.md).
