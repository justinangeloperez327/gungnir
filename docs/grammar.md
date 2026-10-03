# Grammar

> **Status: Gungnir 1.0 current contract.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/grammar.md).

## Current behavior

The accepted frontend grammar is implemented by the lexer/parser and specialized lowerers. It includes transitional native C++ forms alongside supported framework syntax.

## Limits and planned work

The canonical EBNF is the target specification. It is not a claim that the parser rejects every unsupported C++ form or implements every listed declaration, closure, type or modifier. Use transpilation plus native compilation to verify current examples.

## Implementation references

- [src/language/lexer.cpp](../src/language/lexer.cpp)
- [src/language/parser.cpp](../src/language/parser.cpp)
- [include/gungnir/language/token.hpp](../include/gungnir/language/token.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/grammar.md).
