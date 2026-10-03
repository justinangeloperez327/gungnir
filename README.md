# Gungnir

**Gungnir is an expressive C++23 web framework with a structured application language.**

Gungnir targets a Laravel/Adonis-style development experience while retaining native C++ deployment, interoperability, and inspectable generated code.

> Project status: **Development**. Gungnir has no 1.0 release candidate. 1.0 will be assigned only after the completeness gate is satisfied.

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

## Development identity

Development builds intentionally do not claim a public release version.

```text
package_version=development
language_version=development
compiler_contract=development
diagnostic_contract=development
structured_feature_freeze=false
compatibility=experimental
```

CMake uses internal numeric version `0.0.0` only because its package machinery requires a numeric value. It is not a public Gungnir version.

Native development metadata uses:

```text
native API contract = development
native ABI epoch    = 0
release channel     = development
```

See [Development Status](docs/development-status.md).

## Installation

Requirements:

- C++23-compatible compiler;
- CMake 3.25 or newer.

The latest source is the authoritative development build. Historical preview packages remain available from [GitHub Releases](https://github.com/justinangeloperez327/gungnir/releases), but they do not represent the current development contract.

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

Development builds report `Gungnir development`.

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
- logging/observability;
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
- [Support](SUPPORT.md)
- [Security Policy](SECURITY.md)
- [License](LICENSE)

Documentation integrity, development metadata, package consumers, compiler conformance, runtime correctness and the canonical example are continuously validated in CI.
