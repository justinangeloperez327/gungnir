# Compiler Correctness

> **Status: normative engineering contract for the structured compiler.** Gungnir is pre-1.0, but compiler changes are expected to preserve the invariants in this document. The compatibility transpiler remains transitional and is not the long-term correctness boundary.

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
- call and argument binding;
- mutability;
- optional/nullability rules;
- async/await rules;
- return-path analysis;
- framework declaration contracts.

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
- the resulting `CppIrProject` contains everything required for final serialization.

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

## Diagnostic contract

Diagnostics are part of the compiler interface.

For user-caused errors:

- `code` is stable enough for tests and tooling;
- file, line and column refer to `.gnr` source;
- the message states the violated rule;
- generated C++ locations are secondary implementation details.

Ordinary invalid source must not surface as a C++ template error, access violation or unhandled exception.

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

Backend compilation is not a substitute for Gungnir semantic validation.

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
  [ ] validated module order preserved
  [ ] emitter requires no semantic lookup

Code generation
  [ ] deterministic output
  [ ] generated C++ compiles

Execution
  [ ] representative run-pass behavior

Robustness
  [ ] malformed input does not crash
  [ ] regression test added for fixed compiler bug
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

`gungnirc --check` becomes authoritative only when every supported construct can be validated without relying on native compilation to discover ordinary Gungnir type/interface errors.

Until that point, documentation must continue to distinguish structured validation from backend/native verification.

See also:

- [Grammar](grammar.md)
- [Abstract Syntax Tree](ast.md)
- [Semantic Analysis](semantics.md)
- [Validated AST](validated-ast.md)
- [Compiler Conformance](compiler-conformance.md)
- [Transpiler](transpiler.md)
- [Testing](testing.md)
