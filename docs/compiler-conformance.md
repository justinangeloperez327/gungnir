# Compiler Conformance

> **Status: living implementation matrix.** A feature is not considered compiler-complete merely because it parses. Completion requires the relevant syntax, semantic validation, validated representation, code generation, diagnostics and tests.

## Status values

| Status | Meaning |
| --- | --- |
| Complete | Structured path covers the feature and correctness tests exist |
| Partial | Structured support exists but one or more correctness layers remain incomplete |
| Transitional | Compatibility/source-edit behavior still participates materially |
| Planned | Contract exists but implementation is not yet complete |

## Core compiler

| Capability | Current status | Evidence / remaining gate |
| --- | --- | --- |
| Tokenization and source origins | Complete | Lexer/parser diagnostics and source origins are tested |
| Structural parser | Complete for current structured profile | `SyntaxParser` produces declarations, statements and expressions |
| Syntax AST arenas | Complete for current structured profile | `SyntaxProject` owns declarations, expressions and statements with stable IDs |
| Module resolution | Complete for current structured profile | Project compilation tests cover aliases, missing modules, cycles and module mismatch |
| Symbol/name resolution | Complete for current structured profile | `ProgramValidator` produces resolved symbols and rejects unknown names |
| Type analysis | Partial | Current structured types are resolved; advanced null-flow/native overload behavior remains outside the current profile |
| Call/argument binding | Complete for current structured profile | Named/default argument checks and bound argument order are represented before emission |
| Async/await validation | Complete for current structured profile | Invalid await/async calls are rejected before emission |
| Return-path validation | Partial | Required return checks exist; loop termination is not proof of return |
| Framework contract validation | Partial | Core declaration contracts are checked; coverage must expand with the supported surface |
| Validated AST boundary | Complete for structured path | `ValidatedProject` can only be created by validation and is consumed by `CppIrLowerer` |
| Deterministic multi-file compilation | Complete baseline | Source files are sorted before parsing/merging; dedicated correctness tests enforce output stability |
| Dedicated C++ IR boundary | Complete baseline | `Compiler` lowers `ValidatedProject -> CppIrProject`; `CppEmitter` serializes IR only |
| Typed C++ function/statement/expression IR | Complete baseline | Executable implementation bodies use typed target IR; CppEmitter walks structural functions/statements and CppIrVerifier enforces backend invariants |
| Typed C++ declaration IR | Complete baseline | Interface/header output is ordered CppIrDeclaration records for preamble, forwards, class definitions and model metadata; identity/order are verifier-checked |
| Fine-grained C++ class/member IR | Partial | Generated class members retain finalized target spelling inside typed class-definition records; deeper member nodes are optional future refinement |
| Canonical compiler selection | Complete | `gungnirc` defaults to `Compiler`; `--compat` is explicit and profile behavior is CI-tested |
| Structured project route emission | Complete baseline | Route URI/controller/action/middleware are structural legacy-route AST data, semantically checked, then emitted directly without SourceEdit |
| Legacy source-edit elimination | Transitional | `CompatibilityTranspiler` and specialized source/token lowerers remain only for explicit `--compat` legacy/native compatibility |
| Authoritative `--check` | Complete for current structured profile | Phase 6 enforces validation-only/full-pipeline semantic parity before C++ IR |
| Diagnostic/source mapping | Complete baseline | Phase 7 preserves source spans, deterministic diagnostic order, rich CLI rendering and statement-level `#line` mapping |
| Fuzzing and compiler robustness | Complete baseline | Phase 8 adds bounded parser nesting, deterministic adversarial mutations and ASan/UBSan-backed libFuzzer targets for lexer and structured compiler |
| GCC/Clang/MSVC conformance | Complete baseline | Phase 9 builds and runs compiler correctness, robustness and generated structured application tests under all three supported compiler families, then requires byte-identical structured compiler snapshots |

## Feature completion rule

For each supported feature, track the following columns:

| Feature | Parse | AST | Semantics | Validated | Emit | Compile-fail | Run-pass | Status |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Functions | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Complete |
| Variables / mutability | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Complete |
| Named/default arguments | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Complete |
| Lambdas/captures | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Complete |
| Optional types | Yes | Yes | Partial | Yes | Yes | Yes | Yes | Partial |
| Async/await | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Complete |
| Modules/imports | Yes | Yes | Yes | Yes | Yes | Yes | Yes | Complete |
| Models | Yes | Yes | Partial | Yes | Yes | Yes | Yes | Partial |
| Controllers | Yes | Yes | Partial | Yes | Yes | Yes | Yes | Partial |
| Migrations | Yes | Yes | Partial | Yes | Yes | Yes | Yes | Partial |
| Middleware | Yes | Yes | Partial | Yes | Yes | Yes | Yes | Partial |
| Policies | Yes | Yes | Partial | Yes | Yes | Yes | Yes | Partial |
| Events/listeners | Yes | Yes | Partial | Yes | Yes | Yes | Yes | Partial |
| Notifications/mail | Yes | Yes | Partial | Yes | Yes | Yes | Yes | Partial |
| Jobs | Yes | Yes | Partial | Yes | Yes | Yes | Yes | Partial |

“Partial” does not mean unusable. It means the correctness contract has additional semantic or framework-specific cases that must be closed before a 1.0 language guarantee.

## Definition of compiler-complete

A row may move to **Complete** only when:

1. grammar acceptance/rejection is specified;
2. AST shape is structural;
3. semantic rules are explicit;
4. all symbols/types needed by lowering are resolved;
5. emission requires no source-string semantic inference;
6. valid examples compile through a supported C++23 backend;
7. invalid examples fail with Gungnir diagnostics;
8. at least one regression/run-pass test covers representative behavior.

## Maintenance rule

Any compiler PR that changes a row in this table must update the status/evidence in the same pull request. New framework syntax must not be marked complete at parser-only stage.

See [Compiler Correctness](compiler-correctness.md) for the normative invariants.


## Cross-compiler conformance

Phase 9 treats compiler portability as a language contract rather than a packaging check. The targeted conformance matrix covers the three supported C++ compiler families:

| Toolchain | CI platform | Required evidence |
| --- | --- | --- |
| GCC | Ubuntu | compiler correctness, robustness, public headers, generated structured application, deterministic snapshot |
| Clang | Ubuntu | compiler correctness, robustness, public headers, generated structured application, deterministic snapshot |
| MSVC | Windows | compiler correctness, robustness, public headers, generated structured application, deterministic snapshot |

The matrix intentionally does not rebuild every framework integration test on every compiler. Its job is to prove that the language/compiler boundary and emitted C++23 are portable without restoring a slow full-platform test matrix.

Each toolchain produces the same conformance snapshot from the canonical structured fixtures:

- generated C++ for the single-file structured program;
- `ValidatedProject` dump;
- structural C++ IR dump;
- generated C++ for the multi-module fixture;
- SHA-256 manifest.

The comparison job requires these artifacts to be byte-identical across GCC, Clang and MSVC. A toolchain-specific semantic decision, declaration order, type spelling or emitted source difference therefore fails Phase 9 even if all three native compilers happen to accept the output.

Compiler-specific warnings or backend implementation defects are fixed in the originating compiler/runtime layer; they must not be papered over with toolchain-specific generated semantics.
