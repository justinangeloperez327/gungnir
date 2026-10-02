# Validated AST

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/validated-ast.md).

## Current behavior

The structured `Compiler` resolves syntax into `ValidatedProject`. The validated object can only be created by `ProgramValidator`; its syntax, symbol IDs, type IDs, bound arguments, captures and module order are exposed through const accessors. Callable resolution also records whether structured control-flow analysis proves that the callable cannot fall through. `CppEmitter` accepts this object rather than unchecked syntax. Failed validation emits no C++.

## Limits and planned work

The legacy `Transpiler` remains a separate compatibility path. Native APIs require explicit `CompilerOptions` bindings in the structured frontend; it does not infer arbitrary C++ declarations.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/language/ast.hpp](../include/gungnir/language/ast.hpp)
- [include/gungnir/language/semantic.hpp](../include/gungnir/language/semantic.hpp)
- [src/language/transpiler.cpp](../src/language/transpiler.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/validated-ast.md).
