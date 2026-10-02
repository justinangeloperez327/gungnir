# C++ Intermediate Representation

> **Status: implemented backend boundary, pre-1.0.** The Phase 3 C++ IR separates semantic lowering from final C++ text serialization.

## Pipeline

```text
SyntaxProject
    ↓
ProgramValidator
    ↓
ValidatedProject
    ↓
CppIrLowerer
    ↓
CppIrProject
    ↓
CppEmitter
    ↓
C++23
```

The important boundary is between `CppIrLowerer` and `CppEmitter`.

`CppIrLowerer` may inspect validated syntax, resolved types, symbols, bound arguments, captures, framework metadata and module order. `CppEmitter` must not perform those semantic operations. It serializes an already-lowered `CppIrProject`.

## Current IR model

The Phase 3 IR is target-specific and intentionally small:

- `CppIrProject` owns interface, header, implementation and per-module units;
- `CppIrUnit` identifies one generated module translation unit;
- `CppIrFragment` contains a lowered C++ fragment and a fragment kind;
- `CppIrFragmentKind` distinguishes interface declarations, monolithic implementation definitions and module definitions.

This is a real compiler stage because all Gungnir semantic/code-generation decisions occur before the emitter. The current fragments contain lowered C++ text; they are not yet a fully typed C++ expression/statement tree.

That distinction is intentional. Phase 3 establishes the architectural firewall first. Future backend refinement may replace fragment bodies with finer-grained typed C++ IR nodes without changing the `ValidatedProject -> CppIrProject -> CppEmitter` contract.

## Invariants

A valid `CppIrProject` must satisfy:

1. it is created only from a valid `ValidatedProject`;
2. lowering is deterministic for identical validated input and options;
3. module unit order follows validated module order;
4. no unresolved Gungnir symbol, overload, type conversion or framework contract remains for `CppEmitter`;
5. emitter output is a deterministic serialization of IR fragments;
6. generated per-module units preserve the same observable program behavior as monolithic output;
7. the emitter does not parse or inspect original `.gnr` source.

The emitter may add fixed target scaffolding such as `#pragma once` or the generated `program.hpp` include. It may not decide language semantics.

## Public API

```cpp
auto validation = ProgramValidator{}.validate(std::move(syntax));
auto ir = CppIrLowerer{}.lower(*validation.project, false);

auto cpp = CppEmitter{}.emit(ir);
auto units = CppEmitter{}.emit_units(ir);
```

For pre-1.0 source compatibility, `CppEmitter` still accepts `ValidatedProject` directly. Those overloads are wrappers and immediately call `CppIrLowerer`; they do not bypass IR.

## Inspecting IR

Use:

```sh
gungnirc app.gnr --dump-cpp-ir
gungnirc app.gnr --dump-cpp-ir --no-line-directives
gungnirc app --project --dump-cpp-ir
```

The dump is intended for compiler debugging, deterministic regression tests and backend development. It is not a stable application-facing format.

`--dump-cpp-ir` belongs to the structured compiler and cannot be combined with `--compat`.

## Monolithic and project emission

The IR carries both:

- a monolithic interface/implementation representation used by single-file compiler output;
- a header-safe interface plus per-module units used by structured project builds.

The generated header intentionally omits source line directives so source-location-only edits do not unnecessarily invalidate the shared interface. Module implementations may preserve line directives for native compiler diagnostics.

## Correctness testing

`tests/compiler_correctness.cpp` verifies:

- an IR object is produced after validation;
- lowering is deterministic;
- module units match validated module order;
- IR-first emission matches compatibility-wrapper emission;
- generated header and module scaffolding remain stable.

`tests/compiler_profiles.cmake` verifies the `--dump-cpp-ir` CLI contract.

Native generated-program tests continue to verify that serialized IR compiles and executes correctly.

## Remaining backend refinement

The current IR removes semantic responsibilities from `CppEmitter`, but the fragment bodies are still textual C++.

Later refinement can introduce typed IR nodes for:

- types and declarations;
- expressions;
- statements and control flow;
- calls and conversions;
- coroutine operations;
- framework-generated helpers;
- source mapping.

Those changes should refine `CppIrProject`, not move semantic logic back into the emitter.

## Implementation references

- [include/gungnir/language/cpp_ir.hpp](../include/gungnir/language/cpp_ir.hpp)
- [src/language/cpp_ir.cpp](../src/language/cpp_ir.cpp)
- [src/language/emitter.cpp](../src/language/emitter.cpp)
- [Compiler Correctness](compiler-correctness.md)
- [Compiler Conformance](compiler-conformance.md)
