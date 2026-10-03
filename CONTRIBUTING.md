# Contributing to Gungnir

Gungnir is in the 1.0 release-candidate freeze. Contributions must preserve the frozen 1.0 language/compiler/native API contracts unless a release-blocking correction is explicitly approved.

## Before opening a change

Read:

- [Getting Started](docs/getting-started.md)
- [Stability](docs/stability.md)
- [Compiler Correctness](docs/compiler-correctness.md)
- [Native API and ABI Stability](docs/native-api-abi.md)

For language/compiler changes, also read the grammar, semantics, validated AST, C++ IR, and compiler-conformance documentation.

## Development build

Requirements:

- C++23-compatible compiler
- CMake 3.25+
- Ninja is recommended

Configure and build:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGUNGNIR_BUILD_TESTS=ON \
  -DGUNGNIR_BUILD_TOOLS=ON

cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

## Change expectations

A contribution should:

1. preserve the 1.0 language/compiler contract unless the change intentionally advances it;
2. include a focused regression test for behavioral fixes;
3. preserve generated-output determinism;
4. keep public native API changes compatible with the 1.0 contract or document an intentional break;
5. update current documentation when behavior changes;
6. avoid presenting design-only behavior as already implemented;
7. keep platform-specific behavior explicit.

Performance changes should include a benchmark comparison when the affected path is covered by [Phase 16 benchmarks](docs/performance.md).

Security changes should include a focused regression test and update the relevant [security documentation](docs/security-hardening.md) when the external contract changes.

## Pull requests

Keep pull requests scoped around one coherent contract or subsystem. Include:

- what changed;
- why it changed;
- user-visible compatibility impact;
- tests added or updated;
- documentation impact;
- performance impact when relevant.

Do not commit generated build directories, local packages, benchmark output, secrets, credentials, or local environment files.

## Documentation

Run:

```sh
python3 tools/check_docs.py
```

The documentation checker validates the project version references, required ecosystem files, documentation index coverage, canonical example presence, and relative Markdown links.

The canonical minimal application is under [examples/hello](examples/hello/README.md).

## Compatibility discipline

Gungnir 1.0 uses separate contracts for:

- package version;
- structured language;
- compiler semantics;
- diagnostic codes;
- native C++ source API;
- native binary ABI epoch.

Do not infer compatibility in one layer from another. See [Stability](docs/stability.md).

## Reporting security issues

Do not open a public issue for a suspected vulnerability. Follow [SECURITY.md](SECURITY.md).
