# Abstract Syntax Tree

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

## Current behavior

`SyntaxProject` owns declaration, expression and statement arenas with source origins and stable arena IDs. It represents modules/imports, ordinary functions, framework declarations, generic/optional types, fields, metadata, methods, named calls, arrows and control flow.

## Limits and planned work

The previous flat `Program` API remains available for compatibility. General classes, interfaces, enums, native preprocessor directives and arbitrary native C++ expressions are outside the structured grammar.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/language/ast.hpp](../include/gungnir/language/ast.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/ast.md).
