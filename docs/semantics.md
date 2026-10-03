# Semantic Analysis

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/semantics.md).

## Current behavior

`ProgramValidator` performs structured module/name/type binding, member visibility, call/default/named argument checks, mutability, async/await checks, callback contracts, required framework methods and return-path checks. Errors preserve file, line, column and diagnostic codes; invalid programs cannot reach `CppEmitter`.

Phase 5 centralizes common-type selection for conditional expressions and inferred callable/lambda returns. Numeric result typing is deterministic instead of operand-order dependent, nullable inference can form `T?` from `null` plus `T`, and terminating null guards propagate a proven non-null fact into the surviving control-flow path. Writes invalidate those narrowing facts.

Phase 13 extends semantic validation to framework-artifact runtime contracts. Middleware signatures, migration entry points, policy abilities, event/listener/job contracts, notification payload methods, mail composition, and model lifecycle metadata are rejected before C++ IR when their generated runtime surface would be invalid. Focused diagnostics use `GNR2301` through `GNR2308`. Model `timestamps=true` and `softDeletes=true` also synthesize the nullable lifecycle fields required by ORM persistence.

Phase 6 makes structured `gungnirc --check` an authoritative semantic gate for the supported language profile. Validation-only compilation stops at `ValidatedProject` and does not require C++ IR lowering or emission to decide source validity. Return-path analysis now uses structured flow summaries rather than a first-match return scan: return, throw, break, continue and fallthrough are modeled separately, exhaustive branches are recognized, and loops remain conservative unless their termination can be proven without backend assumptions.

## Limits and planned work

Loop termination is intentionally conservative: a loop body that returns does not by itself prove that the loop executes. Null-flow analysis currently covers direct local/parameter null guards and terminating branches; alias-sensitive, member-sensitive and loop-derived narrowing remain outside this profile. Native overload sets remain outside this profile. `SemanticAnalyzer` remains available for the compatibility frontend.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/language/semantic.hpp](../include/gungnir/language/semantic.hpp)
- [src/language/semantic.cpp](../src/language/semantic.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/semantics.md).
