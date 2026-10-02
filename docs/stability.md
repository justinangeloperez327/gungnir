# Stability

> **Status: Gungnir 0.9 preview.** The structured compiler profile is feature-frozen for the 0.9 line. Gungnir remains pre-1.0, and native/runtime compatibility is still experimental.

## Versioned contracts

Gungnir 0.9 separates package, source-language, compiler, and diagnostic compatibility:

| Contract | Version | 0.9 policy |
| --- | --- | --- |
| Package / CLI | 0.9.0 | SemVer package version |
| Structured language | 0.9 | Feature-frozen for the 0.9 line |
| Compiler semantic contract | 0.9 | Changes must preserve accepted/rejected behavior for the frozen profile unless explicitly documented as a correction |
| Diagnostic code contract | 0.9 | Existing diagnostic codes used by tooling/tests must not be casually renumbered |
| Generated C++ ABI/spelling | Unstable | Rebuild with the matching compiler/runtime |
| Native runtime C++ API | Experimental | Source/ABI compatibility is not guaranteed before 1.0 |
| Compatibility transpiler | Transitional | Explicit `--compat` only; not part of the 0.9 structured-language guarantee |

The compiler exposes this metadata through:

```sh
gungnirc --version
gungnirc --print-contract
```

The contract output is machine-readable and verified in CI and release packaging.

## What “feature-frozen” means

The 0.9 structured profile does not accept new syntax or new semantic behavior merely because it can be implemented. During the 0.9 stabilization line, compiler changes should be limited to:

- correctness fixes;
- diagnostic/source-location fixes that preserve diagnostic identity where practical;
- crash, memory-safety, and resource-limit fixes;
- backend portability fixes;
- performance work that does not change source semantics;
- documentation corrections;
- implementation of already-documented behavior that is explicitly classified as part of the frozen profile.

A change that intentionally alters accepted syntax, type rules, control-flow behavior, or framework semantics requires an explicit contract-version decision rather than silently changing 0.9.

## What is not frozen

The feature freeze does **not** claim that every framework feature is complete or that Gungnir is 1.0-stable. The compiler-conformance matrix still identifies partial framework semantics. General classes/interfaces/enums, arbitrary native C++ syntax, and other unsupported structured-language features remain outside the frozen profile.

Generated C++ is an implementation artifact. Applications should not depend on generated namespaces, helper names, class layout, or ABI. Rebuild generated code with the same Gungnir package version used by the runtime.

## Compatibility policy

The structured compiler is the canonical `gungnirc` profile. `--strict` remains a compatibility alias for that default. Legacy/native-compatible source requires explicit `--compat`.

Within the 0.9 line:

- validated source behavior and diagnostic codes are guarded by dedicated stability tests;
- `--check` remains a validation-only semantic gate and stops at `ValidatedProject`;
- GCC, Clang, and MSVC must pass the same compiler stability/correctness corpus;
- canonical structured outputs must remain deterministic across supported toolchains.

Before 1.0, a severe correctness or safety defect may require a breaking correction. Such a correction must be documented rather than hidden behind backend-specific behavior.

## Implementation references

- [Compiler Correctness](compiler-correctness.md)
- [Compiler Conformance](compiler-conformance.md)
- [Compiler Profiles](compiler-profiles.md)
- [Design Stability Contract](design/stability.md)
- [include/gungnir/language/spec.hpp](../include/gungnir/language/spec.hpp)

