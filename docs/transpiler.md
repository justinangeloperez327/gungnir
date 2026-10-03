# Compatibility Transpiler

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

The canonical path is `language::Compiler` and is used by `gungnirc` by default:

```text
.gnr
  -> SyntaxParser
  -> ProgramValidator
  -> ValidatedProject
  -> CppIrLowerer
  -> CppIrProject
  -> CppEmitter
```

Use `gungnirc --compat` only for source that intentionally depends on the earlier native-C++ compatibility grammar.

## Current compatibility behavior

`CompatibilityTranspiler` tokenizes source, parses the legacy `Program`, runs compatibility semantic diagnostics, invokes specialized lowerers and applies `SourceEdit` replacements to the original source.

The historical `Transpiler` C++ name remains as an alias to `CompatibilityTranspiler` so existing 1.0 RC callers continue to build.

## Rules for new compiler work

The compatibility pipeline is frozen for new language semantics.

Do not add new Gungnir language behavior to:

- `ModelLowerer`;
- `ControllerLowerer`;
- `MiddlewareLowerer`;
- `MigrationLowerer`;
- `AsyncLowerer`;
- `ValidationLowerer`;
- `ViewLowerer`;
- generic `SourceEdit` rewriting.

New syntax and framework semantics belong in the structured parser, semantic validator, validated compiler structures and C++ IR lowering. The final emitter must remain semantic-free.

Compatibility fixes are limited to regressions required to keep explicitly opted-in legacy applications working during the 1.0 RC migration window.

## Remaining migration work

Structured application declarations use validated-only emission, and structured project routes are parsed into route nodes, semantically checked against the project index, and emitted directly without SourceEdit. The remaining SourceEdit lowerers are confined to explicit `--compat` use and legacy/native-C++ fixture coverage.

## Implementation references

- [Structured compiler](../include/gungnir/language/compiler.hpp)
- [Compatibility API](../include/gungnir/language/transpiler.hpp)
- [Compatibility implementation](../src/language/transpiler.cpp)

See [Compiler Correctness](compiler-correctness.md), [Compiler Conformance](compiler-conformance.md), and [Compiler Profiles](compiler-profiles.md).
