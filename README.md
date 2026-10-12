# Gungnir

**Gungnir is an expressive C++23 web framework with a structured application language.**

Gungnir targets a Laravel/Adonis-style development experience while retaining native C++ deployment, interoperability, and inspectable generated code.

> Project status: **Stable 1.0.0**. See the [published release scope](docs/release-v1.md) and [current-source status](docs/development-status.md).

## Published release versus current source

The published **v1.0.0** SDKs were released on **5 October 2026**.
Later changes merged to `main` have **not** been added to those existing
release downloads, although the source package version still says `1.0.0`.

| Install | Best for | Documentation |
| --- | --- | --- |
| Published v1.0.0 core SDK | Core HTTP applications and the APIs actually shipped | [Release scope](docs/release-v1.md) |
| SDK built from current `main` | Newer Spear views, typed delivery, application SDK, Redis coordination and installed production/performance examples | [Source availability](docs/development-status.md) and [SDK packages](docs/sdk-packages.md) |
| Custom source-built SDK | Applications requiring native database, Redis, SMTP, TLS or OTLP adapters | [Database](docs/database.md) and adapter-specific guides |

Environment variables alone do not add missing compiled adapters. Select an
SDK that contains the features your application uses.

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

Current source offers an [application SDK](docs/sdk-packages.md) with bundled
SQLite and password-hashing dependencies. The existing published v1.0.0
downloads are **core-only**; build the application SDK from current source
until a separate package is published.

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

Across the published core framework and later merged source, Gungnir implements:

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
- [Spear views](docs/view.md), storage and cache;
- SQLite, PostgreSQL, MySQL/MariaDB, SQL Server and MongoDB adapters;
- async runtime and cancellation;
- HTTP serving and WebSockets;
- health/readiness and graceful shutdown;
- [logging/observability](docs/logging-observability.md) and
  [external collector integration](docs/external-observability.md);
- production resilience and overload admission.

A subsystem is not considered **complete** merely because its type, parser node, interface, or basic implementation exists. Gungnir 1.0 requires end-to-end behavior, tests, consistent DX, backend coverage where applicable, and current documentation.

## Build an application

Follow [Getting Started](docs/getting-started.md) and the verified
[Hello Gungnir example](examples/hello/README.md) for a first native HTTP
endpoint. For pages using the newer Blade-like syntax, follow
[Spear views](docs/view.md) and the [Spear example](examples/spear/README.md),
which require current source.

Use a configured persistence adapter for database-backed applications and
independent workers. For production, explicitly configure TLS/proxying,
shared persistence, process supervision, recovery, backups and monitoring.
The [Linux deployment example](docs/production-deployment.md) illustrates
a current-source installation; it is not part of the October 5 core SDK.

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
- [Linux deployment reference](docs/production-deployment.md)
- [Application load and performance](docs/application-performance.md)
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
