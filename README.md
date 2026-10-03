# Gungnir

**Gungnir is an expressive C++23 web framework with a structured application language.**

Gungnir targets a Laravel/Adonis-style development experience while retaining native C++ deployment, interoperability, and inspectable generated code.

> **Current public preview: v0.9.0.** Gungnir remains pre-1.0. The structured language/compiler contract is feature-frozen at 0.9 while runtime, packaging, documentation, and release readiness continue toward 1.0.

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

Structured `.gnr` source is parsed, semantically validated, lowered through structural C++ IR, and emitted as C++23.

## Installation

Requirements:

- C++23-compatible compiler;
- CMake 3.25 or newer.

Download release packages from [GitHub Releases](https://github.com/justinangeloperez327/gungnir/releases).

### Windows

Use the setup executable:

```text
gungnir-v0.9.0-windows-x86_64-setup.exe
```

The installer adds Gungnir's `bin` directory to the **current user's PATH** without replacing the rest of the user's PATH.

Open a new terminal after installation:

```powershell
gungnir --version
gungnirc --version
gungnirc --print-contract
```

A portable Windows ZIP is also published.

### Linux

Use the portable archive:

```text
gungnir-v0.9.0-linux-x86_64.tar.gz
```

Extract it, add its `bin` directory to `PATH`, and use the installation prefix as `GUNGNIR_CMAKE_PREFIX` when your layout is not automatically discoverable.

Verify:

```sh
gungnir --version
gungnirc --version
gungnirc --print-contract
```

## Quick start

Create a project:

```sh
gungnir new hello
cd hello
```

Build and run:

```sh
gungnir build
gungnir run
```

Development mode rebuilds and restarts after successful changes:

```sh
gungnir dev
```

The generated project defaults to:

```text
http://127.0.0.1:8000
```

See [Getting Started](docs/getting-started.md) and the canonical [Hello Gungnir example](examples/hello/README.md).

## Core workflow

```text
.gnr source
    ↓
Lexer / Parser
    ↓
Syntax AST
    ↓
Semantic + type analysis
    ↓
Validated AST
    ↓
Structural C++ IR
    ↓
C++23 emitter
    ↓
Native compiler
    ↓
Application
```

`gungnirc --check` is the authoritative structured semantic gate. `gungnir build` additionally performs C++ generation and native compilation.

## Framework surface

The 0.9 line includes framework contracts for:

- models and Eloquent-style ORM querying;
- migrations;
- controllers and routing;
- middleware;
- validation;
- authentication and authorization;
- sessions and CSRF;
- events and listeners;
- jobs/queues;
- notifications and mail;
- views;
- SQLite, PostgreSQL, MySQL/MariaDB, SQL Server, and MongoDB adapters/capabilities;
- async runtime and cancellation;
- HTTP serving and WebSockets;
- health/readiness and graceful shutdown;
- logging/observability;
- production resilience and overload admission.

Backend-specific capabilities and dependencies remain explicit. Review the current documentation before assuming parity across every adapter.

## Stability

Gungnir 0.9 separates several compatibility contracts:

| Contract | Version / status |
| --- | --- |
| Package | 0.9.0 |
| Structured language | 0.9 feature-frozen |
| Compiler semantic contract | 0.9 |
| Diagnostic contract | 0.9 |
| Native C++ source API contract | 0.9 |
| Native ABI epoch | 0 |
| Generated C++ ABI/spelling | rebuild with matching package |

Native binary compatibility is scoped to compatible platform/compiler/standard-library ABIs. See [Stability](docs/stability.md) and [Native API and ABI Stability](docs/native-api-abi.md).

## Performance

Phase 16 provides reproducible Release-mode benchmarks for:

- compiler parsing/check/full compilation;
- HTTP parsing and response serialization;
- static and parameterized routing;
- ORM query compilation.

Benchmark values are observational and should only be compared on like-for-like environments. See [Performance Baseline](docs/performance.md).

## Documentation

Start with:

- [Documentation Index](docs/README.md)
- [Getting Started](docs/getting-started.md)
- [CLI and Code Generation](docs/cli-codegen.md)
- [Language Frontend](docs/language.md)
- [ORM](docs/orm.md)
- [HTTP Runtime](docs/http-runtime.md)
- [Production](docs/production.md)
- [Security](docs/security-hardening.md)
- [Stability](docs/stability.md)
- [Upgrading](docs/upgrading.md)

The `docs/design/` directory preserves intended architecture and future-facing design contracts. Do not treat design-only examples as implemented current syntax unless the current guides say so.

## Build from source

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGUNGNIR_BUILD_TOOLS=ON \
  -DGUNGNIR_BUILD_TESTS=OFF

cmake --build build --parallel 2
cmake --install build
```

Optional adapters require their corresponding CMake options and native dependencies.

## Project ecosystem

- [Changelog](CHANGELOG.md)
- [Contributing](CONTRIBUTING.md)
- [Support](SUPPORT.md)
- [Security Policy](SECURITY.md)
- [License](LICENSE)

Documentation integrity and the canonical example are verified in CI so release/version drift and broken public links do not silently accumulate.
