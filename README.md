# Gungnir

**Gungnir is an expressive C++23 web framework with a structured application language.**

Gungnir targets a Laravel/Adonis-style development experience while retaining native C++ deployment, interoperability, and inspectable generated code.

> Project status: **Stable 1.0.0**. See the release scope and known limitations in the [v1.0.0 release guide](docs/release-v1.md).

## What Gungnir looks like

```gnr
model User {
    table = 'users';
    fillable = ['name', 'email'];
    timestamps = true;
}
```

```gnr
controller UserController {
    public index() {
        return json(User::orderBy('name').get());
    }
}
```

```gnr
Route::get('/users', UserController::index)
    .name('users.index');
```

Structured `.gnr` source is parsed, semantically validated, lowered through typed structural C++ IR, and emitted as C++23.

## Release identity

```text
package_version=1.0.0
language_version=1.0
compiler_contract=1.0
diagnostic_contract=1.0
structured_feature_freeze=true
compatibility=stable
```

Native API contract is `1.0`, ABI epoch is `1`, and release channel is `stable`.
Generated application C++ must be rebuilt with the matching compiler/runtime.
See [Stability](docs/stability.md).

## Installation

Requirements:

- C++23-compatible compiler;
- CMake 3.25 or newer.

Install the v1.0.0 Windows setup executable or portable Windows/Linux SDK from [GitHub Releases](https://github.com/justinangeloperez327/gungnir/releases). The packaged core includes the CLI, compiler, headers, libraries and CMake package. Optional adapters require a source build with their dependencies; see [release installation](docs/release-v1.md).

Current source also offers an [application SDK](docs/sdk-packages.md) with bundled
SQLite and password hashing dependencies. Release packaging verifies both SDK
profiles; the existing v1.0.0 core assets remain unchanged.

### Build from source

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGUNGNIR_BUILD_TOOLS=ON \
  -DGUNGNIR_BUILD_TESTS=OFF

cmake --build build --parallel 2
cmake --install build
```

Verify:

```sh
gungnir --version
gungnirc --version
gungnirc --print-contract
```

Version 1.0.0 reports `Gungnir 1.0.0`.

## Quick start

```sh
gungnir new hello
cd hello
gungnir build
gungnir run
```

Development mode:

```sh
gungnir dev
```

The generated project defaults to:

```text
http://127.0.0.1:8000
```

See [Getting Started](docs/getting-started.md) and the canonical [Hello Gungnir example](examples/hello/README.md).

## Canonical compiler architecture

```text
.gnr source
    ↓
Lexer
    ↓
Parser
    ↓
Syntax AST
    ↓
Semantic + type analysis
    ↓
Validated AST
    ↓
Typed structural C++ IR
    ↓
C++23 emitter
    ↓
Native compiler
    ↓
Application
```

`gungnirc --check` is the authoritative structured semantic gate. `gungnir build` additionally performs C++ generation and native compilation.

## Framework surface

The framework already contains substantial implementation across:

- models and ORM;
- migrations;
- controllers and routing;
- middleware;
- validation;
- authentication and authorization;
- sessions and CSRF;
- events and listeners;
- jobs/queues and scheduler;
- notifications and mail;
- views, storage and cache;
- SQLite, PostgreSQL, MySQL/MariaDB, SQL Server and MongoDB adapters;
- async runtime and cancellation;
- HTTP serving and WebSockets;
- health/readiness and graceful shutdown;
- [logging/observability](docs/logging-observability.md) and
  [external collector integration](docs/external-observability.md);
- production resilience and overload admission.

A subsystem is not considered **complete** merely because its type, parser node, interface, or basic implementation exists. Gungnir 1.0 requires end-to-end behavior, tests, consistent DX, backend coverage where applicable, and current documentation.

## 1.0 release rule

Gungnir will become `1.0.0` only after:

1. all intended framework features are complete;
2. compiler architecture is consistent;
3. framework APIs are consistent;
4. database/backend behavior is verified;
5. security and production readiness are verified;
6. integration/stress/fuzz/performance tests are satisfactory;
7. installers/packages are verified;
8. documentation matches implementation;
9. the final completeness audit is fully green.

There is no phase-number-based version promotion.

## Documentation

Start with:

- [Documentation Index](docs/README.md)
- [Development Status](docs/development-status.md)
- [Getting Started](docs/getting-started.md)
- [CLI and Code Generation](docs/cli-codegen.md)
- [Language Frontend](docs/language.md)
- [ORM](docs/orm.md)
- [HTTP Runtime](docs/http-runtime.md)
- [Production](docs/production.md)
- [Security](docs/security-hardening.md)
- [Stability](docs/stability.md)

The `docs/design/` directory contains intended architecture and future-facing design. Current implementation guides and executable tests remain authoritative.

## Project ecosystem

- [Changelog](CHANGELOG.md)
- [Contributing](CONTRIBUTING.md)
- [Engineering Contract Audits](engineering/README.md)
- [Support](SUPPORT.md)
- [Security Policy](SECURITY.md)
- [License](LICENSE)

Documentation integrity, release metadata, package consumers, compiler conformance, runtime correctness and the canonical example are continuously validated in CI.
