# Abstract Syntax Tree

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/ast.md).

## Current behavior

Program owns a flat vector of Node variants. Framework declarations retain member indices; expressions/statements have child structure and source spans. FrameworkMethod/ControllerMethod and model metadata nodes are transitional representations.

## Limits and planned work

There is no complete ModuleUnit tree, stable NodeId arena, dedicated syntax TypeSyntax hierarchy or separate declaration variant for every framework construct. Text/raw expression and flat-index dependencies remain.

## Implementation references

- [include/gungnir/language/ast.hpp](../include/gungnir/language/ast.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/ast.md).
