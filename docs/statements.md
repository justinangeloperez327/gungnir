# Statements

## Overview
MethodStatement represents returns, bindings, expression statements, blocks, conditionals, loops, break and continue. Semantics checks selected scope, mutability, condition, return and loop-control rules.

## Scope
The target grammar is stricter and more complete than current parsing. Full definite assignment and all-path return analysis are not guaranteed for arbitrary native escape forms.



- [include/gungnir/language/ast.hpp](../include/gungnir/language/ast.hpp)
- [src/language/parser.cpp](../src/language/parser.cpp)
- [src/language/semantic.cpp](../src/language/semantic.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/statements.md).
