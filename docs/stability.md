# Stability

Gungnir 1.0.0 freezes the existing published contracts described by the current guides and executable acceptance tests. The [release scope](release-v1.md) lists optional adapters and pending features separately.

| Surface | Contract |
| --- | --- |
| Package | `1.0.0` |
| Structured language/compiler/diagnostics | `1.0` |
| Structured feature freeze | true |
| Compiler compatibility | stable |
| Native C++ source API | `1.0` |
| Native ABI epoch | 1 |
| Generated C++ | regenerate and rebuild with the matching SDK |

Use `gungnirc --print-contract` for machine-readable identity. Correctness, deterministic IR/emission, diagnostics, GCC/Clang/MSVC agreement, runtime/security/ORM regression tests, installed consumers and performance gates remain enabled.

Patch releases preserve source contracts while correcting defects. Minor releases can add compatible capabilities. Breaking changes require a new major contract and migration documentation. A frozen contract describes the accepted language and documented behavior; it does not prohibit compatible extensions.

Native binary compatibility also depends on operating system, architecture, compiler, standard library, runtime and enabled dependencies. Rebuild application code when changing those dimensions. Generated helper names, layout and emitter spelling are implementation details.

See [Native API and ABI](native-api-abi.md), [Compiler Correctness](compiler-correctness.md), [Compiler Conformance](compiler-conformance.md) and [Upgrading](upgrading.md).
