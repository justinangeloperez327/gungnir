# Semantic Analysis

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/semantics.md).

## Current behavior

`ProgramValidator` performs structured module/name/type binding, member visibility, call/default/named argument checks, mutability, async/await checks, callback contracts, required framework methods and return-path checks. Errors preserve file, line, column and diagnostic codes; invalid programs cannot reach `CppEmitter`.

Phase 5 also centralizes common-type selection for conditional expressions and inferred callable/lambda returns. Numeric result typing is deterministic instead of operand-order dependent, nullable inference can form `T?` from `null` plus `T`, and terminating null guards propagate a proven non-null fact into the surviving control-flow path. Writes invalidate those narrowing facts.

Structured `gungnirc --check` uses validation-only compilation: it stops at `ValidatedProject` and does not require C++ IR lowering or emission to decide whether source is semantically valid.

## Limits and planned work

Loop termination is not treated as proof of a return path. Null-flow analysis currently covers direct local/parameter null guards and terminating branches; alias-sensitive, member-sensitive and loop-derived narrowing remain outside this profile. Native overload sets remain outside this profile. `SemanticAnalyzer` remains available for the compatibility frontend.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/language/semantic.hpp](../include/gungnir/language/semantic.hpp)
- [src/language/semantic.cpp](../src/language/semantic.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/semantics.md).
