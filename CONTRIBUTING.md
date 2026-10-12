# Contributing to Gungnir

Gungnir is a C++23 web framework with a structured `.gnr` application
language. Contributions should keep the documented framework APIs consistent
and the generated applications buildable.

## Get started

Read [Getting Started](docs/getting-started.md),
[CLI and Code Generation](docs/cli-codegen.md),
[Language](docs/language.md), and [Testing](docs/testing.md).

You will need a C++23-compatible compiler, CMake 3.25 or newer, and
the development dependencies required by the components you modify.
Ninja is recommended.

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGUNGNIR_BUILD_TESTS=ON \
  -DGUNGNIR_BUILD_TOOLS=ON

cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

For source validation, use `gungnirc <file.gnr> --check`.
For application behavior, test a built project and the relevant adapters.

## Pull requests

Keep changes focused. Explain the behavior being changed and include
regression tests for bug fixes. Preserve compiler determinism and the
documented language/native compatibility contract; breaking changes need
explicit upgrade guidance.

Update the corresponding [user documentation](docs/README.md) when
commands, APIs or configuration change. Document real supported behavior,
including required dependencies and limitations.

Do not commit generated build directories, local packages, test outputs,
secrets, credentials or local environment files.

## Documentation checks

```sh
python3 tools/check_docs.py
```

The documentation check validates relative links, the documentation index,
release metadata and the canonical
[Hello Gungnir example](examples/hello/README.md).

See [Stability](docs/stability.md) for API compatibility and
[Security](SECURITY.md) for responsible vulnerability reporting.
