# Functions

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/functions.md).

## Current behavior

The current AST represents typed ControllerMethod and FrameworkMethod nodes with parameters and statement bodies.

## Limits and planned work

There is no dedicated top-level FunctionDeclaration variant in Program::nodes. The target function ReturnType name(...) syntax, visibility rules and default arguments are a planned language contract; native C++ callable interoperability is separate.

## Implementation references

- [include/gungnir/language/ast.hpp](../include/gungnir/language/ast.hpp)
- [src/language/parser.cpp](../src/language/parser.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/functions.md).
