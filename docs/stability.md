# Stability

> **Status: pre-1.0 development.** Gungnir is completing the intended framework feature surface before freezing public compatibility contracts.

## Current contract state

| Surface | Current status |
| --- | --- |
| Package / CLI | `0.0.0-dev` internal development identity |
| Structured language | `development`; open to completion work |
| Compiler semantic contract | `development` |
| Diagnostic contract | `development` |
| Native runtime C++ API | `development` |
| Native ABI epoch | `0`; unstable |
| Generated C++ | Rebuild artifact; regenerate with the matching source/package |

`gungnirc --print-contract` reports the same machine-readable state:

```text
package_version=0.0.0-dev
language_version=development
compiler_contract=development
diagnostic_contract=development
structured_feature_freeze=false
compatibility=experimental
```

## What this means

The framework is not feature-frozen. Language syntax, framework declarations, compiler semantics, diagnostics, native APIs, generated code, and ABI details may still change where required to complete the intended 1.0 feature set and make behavior consistent across the framework.

This is not permission for arbitrary churn. Changes should still be deliberate, tested, documented, and aligned with the framework's Laravel/Adonis-style developer experience.

## When 1.0 can freeze

The 1.0 contracts should be frozen only after:

1. the intended framework features are implemented end-to-end;
2. cross-component conventions and behavior are consistent;
3. supported database/runtime adapters meet their documented contracts;
4. current documentation and examples match real APIs;
5. compiler/runtime correctness and resilience gates are green;
6. installation, packaging, and generated-project workflows are verified.

Only then should the project assign stable 1.0 language/compiler/native API contracts, advance the ABI epoch, and publish `v1.0.0`.

See [Pre-1.0 Development Status](release-candidate.md), [Native API and ABI Stability](native-api-abi.md), and [Upgrading](upgrading.md).
