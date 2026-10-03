# Stability

> **Status: Gungnir 1.0 release candidate.** The structured language, compiler semantic contract, diagnostic contract, and representative native C++ source API are frozen for the 1.x line. Generated C++ remains a rebuild artifact, and native binary compatibility remains scoped by the C++ ABI environment.

## Versioned contracts

| Contract | RC value | 1.x policy |
| --- | --- | --- |
| Package / CLI | 1.0.0-rc.1 | SemVer; stable release promotion removes only the prerelease marker |
| Structured language | 1.0 | Feature-frozen for the 1.0 RC/stable boundary |
| Compiler semantic contract | 1.0 | Accepted/rejected behavior changes require an explicit compatibility decision |
| Diagnostic code contract | 1.0 | Existing documented/tooling diagnostic codes are stable within 1.x |
| Native runtime C++ API | 1.0 | Stabilized public source signatures follow 1.x compatibility |
| Native runtime ABI epoch | 1 | Binary compatibility also requires compatible platform/toolchain/stdlib/runtime ABI |
| Generated C++ ABI/spelling | Rebuild artifact | Regenerate/rebuild with the matching Gungnir package |
| Compatibility transpiler | Transitional | Explicit `--compat`; not part of the structured 1.0 language guarantee |

The compiler exposes:

```sh
gungnirc --version
gungnirc --print-contract
```

The RC reports:

```text
package_version=1.0.0-rc.1
language_version=1.0
compiler_contract=1.0
diagnostic_contract=1.0
structured_feature_freeze=true
compatibility=stable
```

Native consumers use `<gungnir/version.hpp>` and installed CMake metadata for release version, native API contract, and ABI epoch.

## What feature-frozen means

During the RC and stable 1.0 boundary, changes should be limited to:

- correctness and security fixes;
- diagnostic/source-location corrections that preserve diagnostic identity where practical;
- crash, memory-safety, and resource-limit fixes;
- backend/toolchain portability fixes;
- packaging and installation fixes;
- performance fixes that preserve public semantics;
- documentation corrections;
- release automation/test fixes.

New syntax, framework declarations, semantic expansion, and public API redesign should wait for a later contract/version line.

## Source compatibility

The structured compiler is the canonical `gungnirc` profile. `--strict` remains a compatibility alias for that default. Legacy/native-compatible source requires explicit `--compat`.

The 1.x contract is protected by:

- compile-pass/fail semantic stability tests;
- frozen diagnostic-code assertions;
- authoritative `--check` parity;
- GCC, Clang, and MSVC conformance;
- byte-identical compiler snapshots;
- public native API signature assertions;
- installed-package consumer tests;
- Linux/Windows shared-library consumer tests.

A severe correctness or security issue may require a behavior correction. Such a correction must be documented rather than hidden.

## Native ABI scope

ABI epoch 1 does not make unrelated C++ ABI environments interchangeable. Compatibility still depends on operating system, architecture, compiler ABI, standard library ABI, runtime model, dependency ABI, and relevant feature/build options.

See [Native API and ABI Stability](native-api-abi.md).

## Generated C++

Generated C++ is intentionally inspectable but is not a stable source or binary API. Applications should not depend on generated namespaces, helper names, class layout, or emitted spelling.

Always regenerate and rebuild application code with the matching framework/compiler package. Generated structured output embeds a compiler/runtime contract assertion so mismatches fail during native compilation.

## Release-candidate policy

Phase 19 freezes the intended 1.0 contracts. [Release Candidate](release-candidate.md) defines the release-blocking gates and allowed RC changes.

Phase 20 should only remove the prerelease marker, update release-facing text, verify all gates, and tag/publish `v1.0.0`.

## Implementation references

- [Compiler Correctness](compiler-correctness.md)
- [Compiler Conformance](compiler-conformance.md)
- [Compiler Profiles](compiler-profiles.md)
- [Native API and ABI Stability](native-api-abi.md)
- [1.0 Release Candidate](release-candidate.md)
- [Upgrading](upgrading.md)
- [include/gungnir/language/spec.hpp](../include/gungnir/language/spec.hpp)
