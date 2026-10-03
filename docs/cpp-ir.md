# C++ Intermediate Representation

> **Status: structural backend IR, 1.0 RC.** Phase 4 replaces monolithic implementation fragments with typed target IR for executable functions, statements, expressions and module ownership.

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
CppIrVerifier
    ↓
CppEmitter
    ↓
C++23
```

The backend has two strict boundaries:

- `CppIrLowerer` translates validated Gungnir semantics into target-specific IR.
- `CppEmitter` serializes verified IR and must not inspect `SyntaxProject`, `ValidatedProject`, symbols, overloads or framework semantic contracts.

## Structural IR model

Executable code is represented by typed nodes:

- `CppIrType` — resolved target type spelling;
- `CppIrExpression` — typed expression kind, operands, lambda body links and finalized target spelling;
- `CppIrStatement` — binding, expression, return/co_return, throw, block, if, loops and loop control;
- `CppIrParameter` — target parameter type/name/reference contract;
- `CppIrFunction` — module, owner, result type, coroutine flag, parameters, source mapping and body;
- `CppIrDeclaration` — ordered target declaration record for the preamble, function/class forwards, class definitions and generated model metadata;
- `CppIrUnit` — deterministic module-to-function ownership;
- `CppIrProject` — complete target program IR.

The old monolithic `CppIrFragment` and generic support-block representations are removed.

Interface output is now ordered declaration IR. Class definitions still carry finalized target spelling for their generated framework members, but their declaration kind, module, identity and deterministic ordering are explicit and independently verifiable.

## Expression contract

Every lowered expression records:

- an explicit `CppIrExpressionKind`;
- a resolved target `CppIrType`;
- structural operand IDs;
- structural lambda-body statement IDs where applicable;
- finalized target spelling.

The spelling is a backend cache produced by lowering. It is not raw `.gnr` source and may not be inspected to rediscover Gungnir semantics.

Current expression kinds cover:

```text
literal
name
member
call
unary
binary
subscript
list
object
lambda
await
conditional
conversion
```

Conversions are explicit IR nodes when assignment/binding/return lowering requires a target conversion.

## Statement contract

Executable control flow is no longer stored as a text fragment. `CppEmitter` reconstructs it from:

```text
binding
expression
return
co_return
throw
block
if
while
for
for_in
break
continue
```

This allows the backend verifier to reason about coroutine and control-flow legality before serialization.

## CppIrVerifier

`CppIrVerifier` is the backend invariant gate.

It validates:

- expression and statement IDs;
- target types and expression target spelling;
- expression graph cycles;
- conversion and await operand counts;
- await legality relative to coroutine context;
- return versus co_return legality;
- binding types and initializers;
- loop conditions and loop-control placement;
- for-loop step expression shape;
- function names/result/parameters;
- module-unit ownership;
- exactly-once unit membership for every generated function;
- declaration spelling, identity and ordering;
- matching interface/header declaration shapes and exactly one preamble.

A verifier failure is an internal compiler error. It is not a user program diagnostic.

## Emission

`CppEmitter` walks verified target IR.

For executable code it serializes:

```text
CppIrFunction
    ↓
CppIrStatement
    ↓
CppIrExpression
    ↓
C++23
```

It no longer receives or concatenates monolithic implementation fragments.

The emitter may add fixed target scaffolding such as `#pragma once`, `program.hpp` includes, namespaces and line directives. It may not perform Gungnir name resolution, type checking, overload resolution, optional-flow analysis or framework validation.

## Public API

```cpp
auto validation = ProgramValidator{}.validate(std::move(syntax));

auto ir = CppIrLowerer{}.lower(
    *validation.project,
    false
);

auto verification = CppIrVerifier{}.verify(ir);
if (!verification.success()) {
    // internal compiler error
}

auto cpp = CppEmitter{}.emit(ir);
auto units = CppEmitter{}.emit_units(ir);
```

The 1.0 RC `CppEmitter(ValidatedProject)` overload remains a convenience wrapper. It lowers through structural IR and does not bypass the backend boundary.

## Inspecting IR

Use:

```sh
gungnirc app.gnr --dump-cpp-ir
gungnirc app.gnr --dump-cpp-ir --no-line-directives
gungnirc app --project --dump-cpp-ir
```

The dump now identifies structural functions, statement IDs, expression kinds/types and module-unit membership instead of reproducing generated C++ fragments.

The dump is an experimental compiler-debug format, not a stable application ABI.

## Determinism

For identical validated input and compiler options:

- function order is deterministic;
- statement/expression graph construction is deterministic;
- module units follow validated module order;
- every function belongs to exactly one unit;
- `dump_cpp_ir()` is deterministic;
- final generated C++ is deterministic.

## Remaining backend refinement

Phase 4 completes the structural executable-body boundary.

Remaining backend refinement is intentionally narrower:

1. refine class-definition target spelling into field/member-level nodes only where it improves verification or tooling;
2. progressively replace cached expression target spelling with finer target-expression fields where that materially improves optimization or verification;
3. remove the 1.0 RC `CppEmitter(ValidatedProject)` convenience overload once internal callers use IR directly.

Do not introduce an LLVM-like optimizer unless concrete Gungnir requirements justify it. After declaration-side cleanup, compiler effort should return to semantic/type completeness and authoritative `--check`.

## Correctness testing

`tests/compiler_correctness.cpp` verifies:

- typed function/statement/expression IR exists;
- IR lowering and dumps are deterministic;
- valid IR passes `CppIrVerifier`;
- malformed module/function/type/coroutine IR is rejected;
- IR-first emission equals the compatibility wrapper;
- per-module generated units remain valid.

`tests/compiler_profiles.cmake` verifies the structural `--dump-cpp-ir` CLI contract.

Native generated-program tests continue to verify that structural IR serialization compiles and executes against the framework runtime.

## Implementation references

- [include/gungnir/language/cpp_ir.hpp](../include/gungnir/language/cpp_ir.hpp)
- [src/language/cpp_ir.cpp](../src/language/cpp_ir.cpp)
- [src/language/cpp_ir_verifier.cpp](../src/language/cpp_ir_verifier.cpp)
- [src/language/emitter.cpp](../src/language/emitter.cpp)
- [Compiler Correctness](compiler-correctness.md)
- [Compiler Conformance](compiler-conformance.md)

## Source mapping

When source mapping is enabled, structural statements retain their `CppIrSource` origin and the emitter writes `#line` directives at callable and statement boundaries. This keeps native compiler diagnostics anchored to the closest originating `.gnr` line without using generated source text for semantic decisions.
