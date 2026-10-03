# Functions

## Overview
Use `function int add(int left, int right = 2) { return left + right; }` with `gungnirc --strict`. Functions require declared return types. Calls support literal defaults and named arguments; supplied arguments are evaluated in source order. Framework methods infer their documented response/decision/void contracts. Public, protected and private members are checked.

## Scope
Defaults currently require literal constants. Native overload resolution, variadic functions and general class inheritance are outside this profile. Async calls require explicit `await` inside an async callable.



- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/language/ast.hpp](../include/gungnir/language/ast.hpp)
- [src/language/parser.cpp](../src/language/parser.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/functions.md).
