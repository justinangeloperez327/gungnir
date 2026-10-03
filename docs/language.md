# Language Frontend

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

## Current behavior

The frontend supports first-class framework declarations, typed framework methods, inferred bindings, selected expressions/statements and specialized framework lowering. Native C++ interoperability remains part of the transitional implementation.

## Limits and planned work

The former all-in-one guide is preserved as design/reference material. Its 1.0 wording is not a stable release promise. Start with the current controller/model guides and getting-started walkthrough.

## Implementation references

- [include/gungnir/language/spec.hpp](../include/gungnir/language/spec.hpp)
- [src/language/parser.cpp](../src/language/parser.cpp)
- [src/language/transpiler.cpp](../src/language/transpiler.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/language.md).
