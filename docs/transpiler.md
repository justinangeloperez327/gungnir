# Compatibility Transpiler

> **Status: transitional, pre-1.0.** The source-edit transpiler exists only for legacy/native-compatible Gungnir applications. It is not the canonical language compiler.

The canonical path is `language::Compiler` and is used by `gungnirc` by default:

```text
.gnr
  -> SyntaxParser
  -> ProgramValidator
  -> ValidatedProject
  -> CppEmitter
```

Use `gungnirc --compat` only for source that intentionally depends on the earlier native-C++ compatibility grammar.

## Current compatibility behavior

`CompatibilityTranspiler` tokenizes source, parses the legacy `Program`, runs compatibility semantic diagnostics, invokes specialized lowerers and applies `SourceEdit` replacements to the original source.

The historical `Transpiler` C++ name remains as an alias to `CompatibilityTranspiler` so existing pre-1.0 callers continue to build.

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

New syntax and framework semantics belong in the structured parser, semantic validator, validated compiler structures and emitter.

Compatibility fixes are limited to regressions required to keep explicitly opted-in legacy applications working during the pre-1.0 migration window.

## Remaining migration work

Structured application declarations already use validated-only emission. Remaining compatibility islands, such as legacy route-source handling and native-C++ fixture tests, must remain explicit and should be migrated or retired rather than expanded.

## Implementation references

- [Structured compiler](../include/gungnir/language/compiler.hpp)
- [Compatibility API](../include/gungnir/language/transpiler.hpp)
- [Compatibility implementation](../src/language/transpiler.cpp)

See [Compiler Correctness](compiler-correctness.md), [Compiler Conformance](compiler-conformance.md), and [Compiler Profiles](compiler-profiles.md).
