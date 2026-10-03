# Stability

> **Status: Development.** Gungnir has not frozen its 1.0 compatibility contract. Current behavior is protected by correctness and regression tests, but language, compiler, native API, and ABI surfaces may still evolve while feature completeness is being finished.

## Current development contracts

| Surface | Current status |
| --- | --- |
| Package identity | `development` |
| Internal CMake placeholder | `0.0.0` |
| Structured language contract | `development` |
| Compiler semantic contract | `development` |
| Diagnostic contract | `development` |
| Structured feature freeze | false |
| Compiler compatibility | experimental |
| Native C++ source API | `development` |
| Native ABI epoch | 0 |
| Generated C++ | rebuild with the matching development package |

The compiler exposes:

```sh
gungnirc --version
gungnirc --print-contract
```

Current development metadata is machine-readable so CI can detect accidental premature release claims.

## What is protected during development

Development status does not mean correctness is optional. Existing regression suites protect:

- accepted/rejected compiler behavior;
- diagnostics and source mapping;
- validated AST and structural C++ IR;
- deterministic compiler output;
- GCC, Clang and MSVC conformance;
- runtime lifecycle behavior;
- database/ORM correctness;
- security boundaries;
- production resilience;
- installed package consumers;
- benchmark integrity;
- documentation integrity.

These are implementation contracts under active development, not final 1.0 compatibility promises.

## Feature evolution

The feature set is intentionally not frozen.

New work is allowed when it moves Gungnir toward the defined framework-completeness target and follows the canonical architecture.

Structured language work must continue through:

```text
lexer
-> parser
-> syntax AST
-> semantics
-> validated AST
-> typed C++ IR
-> C++23 emitter
```

New canonical features must not be added as raw source rewriting shortcuts.

## Native API and ABI

Native source APIs remain under development. Compatibility-friendly evolution is preferred, but source compatibility is not promised until the final 1.0 audit.

ABI epoch 0 identifies development binaries only. Rebuild native applications when the framework changes.

See [Native API and ABI Stability](native-api-abi.md).

## Generated C++

Generated C++ remains an implementation artifact. Regenerate and rebuild it with the matching development compiler/runtime.

Applications should not depend on generated helper names, namespaces, layout, or emitter spelling.

## 1.0 transition

There is no release-candidate countdown.

Gungnir moves directly from **Development** to **1.0.0** only after the framework completeness matrix, consistency audit, production audit, integration tests, package verification, performance/stress/fuzz validation, and documentation audit are complete.

At that point, one coordinated change will assign:

- package `1.0.0`;
- language/compiler/diagnostic contract `1.0`;
- native API contract `1.0`;
- ABI epoch `1`;
- feature freeze `true`;
- compatibility `stable`.

See [Development Status](development-status.md).

## Implementation references

- [Development Status](development-status.md)
- [Compiler Correctness](compiler-correctness.md)
- [Compiler Conformance](compiler-conformance.md)
- [Compiler Profiles](compiler-profiles.md)
- [Native API and ABI Stability](native-api-abi.md)
- [Upgrading](upgrading.md)
