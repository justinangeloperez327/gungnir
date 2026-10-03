# Compiler Correctness

> **Status: normative 1.0 compiler contract.** Compiler changes must preserve the invariants in this document. The compatibility transpiler remains transitional and is not the structured-language correctness boundary.

## Definition

A correct Gungnir compiler must satisfy all of the following:

1. **Specification conformance** — accepted source follows the documented language contract.
2. **Static soundness** — accepted programs satisfy name, type, control-flow, async and framework rules.
3. **Semantic preservation** — lowering and emitted C++ preserve the observable meaning of validated Gungnir source.
4. **Determinism** — identical source, compiler version, options and target produce equivalent validated structures and emitted code.
5. **Diagnostic correctness** — invalid programs are rejected with stable diagnostics anchored to Gungnir source.
6. **Robustness** — arbitrary user input must not crash, hang or corrupt the compiler.

Native C++ compilation is a backend verification layer. It must not be used as the primary type checker for ordinary Gungnir application errors.

## Authoritative pipeline

```text
.gnr source
    |
    v
Lexer
    |
    v
Token stream
    |
    v
Parser
    |
    v
SyntaxProject
    |
    v
Module / symbol resolution
    |
    v
Semantic + type analysis
    |
    v
Control-flow + framework validation
    |
    v
ValidatedProject
    |
    v
Framework lowering
    |
    v
C++ representation / emitter
    |
    v
C++23 compiler
    |
    v
Application
```

The structured compiler follows one rule:

> **Parse once, resolve once, validate once, then lower deterministic compiler structures.**

After parsing, later phases must not inspect raw source text to rediscover program semantics. Source text and source spans may remain available for diagnostics and source mapping only.

## Phase invariants

### Lexer

Input: UTF-8 source.

Required postconditions:

- every token has a valid source range;
- keywords, identifiers and literals are classified deterministically;
- malformed tokens produce diagnostics;
- end-of-file is represented consistently;
- arbitrary input does not crash or loop indefinitely.

The parser must not need to reclassify keyword text.

### Parser

Input: token stream.

Required postconditions:

- valid grammar produces a structural `SyntaxProject`;
- invalid grammar produces syntax diagnostics;
- declarations, statements and expressions preserve source origins;
- AST node relationships use valid arena IDs;
- the parser performs syntax recognition, not type checking or framework lowering.

A parser error must never be represented as a malformed tree that later phases are expected to repair.

### Syntax AST

`SyntaxProject` represents what the programmer wrote.

Required postconditions:

- all supported declarations are structural nodes;
- calls, members, operators, literals, lambdas and control flow are structural expressions/statements;
- later semantic phases do not need regexes, token rescans or source-string matching to understand supported syntax;
- source origins remain available for diagnostics.

### Semantic analysis

Semantic analysis owns:

- module resolution;
- symbol resolution;
- scope and visibility;
- type checking;
- common-type selection and safe implicit conversions;
- call and argument binding;
- mutability;
- optional/nullability and flow narrowing rules;
- async/await rules;
- return-path analysis;
- framework declaration contracts.

Type inference and operator result types must be deterministic and independent of operand/branch order. Narrowing facts must be invalidated by writes that can break the proof.

A syntactically valid but semantically invalid program must stop here.

### Validated AST

`ValidatedProject` is the compiler correctness firewall.

A validated project must contain no unresolved semantic questions:

- every expression has a resolved type;
- every resolved name/call/member references a valid symbol;
- argument order and required conversions are fixed;
- captures are fixed;
- module/declaration order is fixed;
- framework contracts have been validated;
- invalid programs cannot construct a `ValidatedProject`.

Only `ProgramValidator`/the semantic validation pipeline may create this state.

### Lowering

Lowering may transform representation but must not invent language semantics.

Required properties:

- input is validated compiler structure, never unchecked syntax;
- no raw-source semantic rescanning;
- no semantic regex matching;
- no re-resolution of names or overloads;
- transformations preserve source-language meaning;
- failures caused by ordinary user mistakes are diagnostics before lowering, not backend compiler errors.

### C++ IR

`CppIrLowerer` is the only structured backend stage allowed to translate validated Gungnir semantics into target-specific C++ operations/fragments.

Required properties:

- input is `ValidatedProject`;
- lowering is deterministic;
- module ordering is preserved;
- semantic binding and conversion decisions are copied from validated state rather than recomputed;
- the resulting `CppIrProject` contains everything required for final serialization;
- executable functions, statements and expressions are structural target IR rather than monolithic implementation strings;
- interface output is ordered structural declaration IR with explicit kind, module and identity; generated member spelling remains target-side data, not a semantic lookup surface.

`CppIrVerifier` must reject malformed target graphs, invalid coroutine/control-flow state, invalid target types/IDs and inconsistent module ownership before emission.

See [C++ IR](cpp-ir.md).

### C++ emission

`CppEmitter` is mechanical. Its canonical input is `CppIrProject`.

It may decide C++ spelling, namespaces, helper names and source mapping, but must not decide:

- whether a symbol exists;
- which overload is selected;
- whether a value is nullable;
- whether `await` is legal;
- whether a framework declaration satisfies its contract.

Those questions must already be encoded in validated compiler structures.

## Acceptance classes

Every supported language feature must have tests in the appropriate classes.

### Compile-pass

Valid programs must parse, validate and emit successfully.

### Compile-fail

Invalid programs must fail at the intended compiler phase and produce the expected diagnostic code/source location.

### Run-pass

Representative programs must compile through the native compiler and execute with the expected behavior.

### Regression

Every compiler defect should receive a minimal permanent regression test when practical.

### Robustness

Malformed and adversarial inputs must result in diagnostics or controlled internal errors, never memory corruption, assertion leakage, hangs or unhandled exceptions.

The structured parser enforces a bounded syntax nesting depth and reports `GNR2004` when the limit is exceeded. Compiler robustness is exercised through both a deterministic mutation corpus in the normal test gate and libFuzzer campaigns instrumented with AddressSanitizer and UndefinedBehaviorSanitizer. Any discovered crash input must become a permanent regression seed or focused test before the fix is considered complete.

## Diagnostic contract

Diagnostics are part of the compiler interface.

For user-caused errors:

- `code` is stable enough for tests and tooling;
- file, line and column refer to `.gnr` source;
- diagnostics preserve a half-open source span when the responsible syntax is known;
- compiler results enrich spans with deterministic end positions and source-line context;
- diagnostic ordering is deterministic across identical multi-file inputs;
- the message states the violated rule;
- generated C++ locations are secondary implementation details.

`gungnirc` must use the shared diagnostic renderer so CLI output and compiler tooling agree on code, source location and highlighting. Ordinary invalid source must not surface as a C++ template error, access violation or unhandled exception.

## Determinism contract

For identical source files, compiler version and `CompilerOptions`:

- module order is deterministic;
- declaration order is deterministic;
- validated symbol/type relationships are deterministic;
- emitted code is deterministic apart from explicitly documented target-dependent output.

Filesystem discovery must be normalized before semantic processing. Unordered containers must not leak nondeterministic ordering into externally observable compiler output.

## Native backend verification

Generated C++ must be compiled in CI with supported native toolchains. Backend compilation verifies:

- the emitter produces legal C++23;
- framework/runtime APIs match generated code;
- no compiler-specific extension is accidentally required.

Backend compilation is not a substitute for Gungnir semantic validation. When line directives are enabled, generated callable signatures and executable statements are mapped back to their originating `.gnr` lines so backend conformance failures remain actionable.

### Cross-compiler contract

For the supported structured profile, GCC, Clang and MSVC must agree at two boundaries:

1. the same compiler correctness and robustness corpus must pass;
2. the canonical fixtures must produce byte-identical validated dumps, C++ IR dumps and generated C++ when line directives are disabled.

Generated semantics must not depend on preprocessor checks for a specific compiler family. Toolchain-specific runtime or build plumbing is allowed only where the native platform requires it and must not change Gungnir language behavior.

## Correctness gate

Compiler-facing pull requests should satisfy the following before merge:

```text
Lexer
  [ ] token/diagnostic tests

Parser
  [ ] grammar tests
  [ ] structural AST tests

Semantics
  [ ] positive validation tests
  [ ] negative validation tests
  [ ] stable diagnostic assertions

Validated AST
  [ ] no unresolved expression types
  [ ] resolved symbols/bindings are valid
  [ ] deterministic module/declaration order

C++ IR
  [ ] deterministic lowering
  [ ] typed executable function/statement/expression graph
  [ ] verifier succeeds
  [ ] validated module order preserved
  [ ] emitter requires no semantic lookup

Code generation
  [ ] deterministic output
  [ ] generated C++ compiles

Execution
  [ ] representative run-pass behavior

Robustness
  [ ] malformed input does not crash
  [ ] parser nesting/resource guards remain enforced
  [ ] deterministic robustness corpus passes
  [ ] sanitizer-backed fuzz targets build and run
  [ ] regression test/seed added for fixed compiler bug
```

The fast PR gate may run a focused subset. Cross-toolchain and expensive integration suites may run separately, but release candidates must pass the complete correctness matrix.

## Internal compiler errors

An internal compiler error means the implementation violated one of its own invariants.

User mistakes must produce normal diagnostics. Assertions, invalid arena access, `std::bad_variant_access`, segmentation faults and unhandled exceptions caused by source input are compiler defects.

## Relationship to compatibility mode

`language::Compiler` is the canonical compiler and `gungnirc` selects it by default. The source-edit path is exposed only as `CompatibilityTranspiler` / `gungnirc --compat`. New language semantics must be implemented in the structured pipeline.

The migration target is:

```text
SyntaxProject
    -> ProgramValidator
    -> ValidatedProject
    -> structured lowering/emission
```

Compatibility lowering must not become the specification for new language behavior. Production code that still requires the compatibility pipeline must opt into it explicitly so remaining migration islands are visible.

## Release criterion

For the current structured language profile, `gungnirc --check` is a validation-only semantic gate: it stops at `ValidatedProject` and does not use C++ IR emission or native compilation to decide source validity.

The profile is authoritative only for constructs the structured compiler claims to support. Unsupported language/framework behavior must be rejected explicitly rather than accepted and deferred to generated C++ diagnostics. Return-path validity is decided from structured control-flow outcomes before lowering; loops are conservative and do not prove callable completion merely because their body terminates. Native compilation remains a backend conformance test, not a semantic fallback.

Phase 6 also requires semantic-gate parity: for the supported profile, validation-only checking and normal compilation must make the same accept/reject decision and produce the same semantic diagnostic codes. Backend IR verification may still detect internal compiler defects, but it must not be needed to diagnose ordinary source errors.

## 1.0 feature-freeze gate

The structured compiler contract is versioned as 1.0 and is feature-frozen for the 1.0 RC/stable line.

Compiler-facing changes must preserve the frozen acceptance/diagnostic corpus unless the change is an intentional correctness correction. The stability gate verifies representative accepted programs, stable diagnostic codes, `--check`/full-compilation semantic parity, and machine-readable compiler contract metadata. The same gate runs under GCC, Clang, and MSVC.

Feature freeze does not stabilize generated C++ ABI, native runtime ABI, compatibility mode, or capabilities still documented as unsupported/partial. Generated structured C++ embeds the compiler contract version and statically rejects a runtime/header set advertising a different contract.

See also:

- [Grammar](grammar.md)
- [Abstract Syntax Tree](ast.md)
- [Semantic Analysis](semantics.md)
- [Validated AST](validated-ast.md)
- [Compiler Conformance](compiler-conformance.md)
- [Transpiler](transpiler.md)
- [Testing](testing.md)
