# Changelog

The changelog describes the capabilities available in Gungnir versions.
For installation and usage instructions, see the [README](README.md) and
[Documentation](docs/README.md).

## Unreleased

- Canonicalized default table naming across the structured compiler and model
  lowering (including acronyms and names ending in `z`). Explicit `table`
  declarations preserve previously deployed table names.
- Reject duplicate model attribute/cast metadata and casts that conflict with
  explicitly typed fields (`GNR2308`), with Windows/Linux compiler tests.
- Diagnose generated ORM metadata member collisions with model fields, methods,
  and relationships before emitting C++ (`GNR2308`).
- Guard recursive structured parsing and expression AST depth at a portable
  limit of 48 (`GNR2004`) to avoid Windows parsing and semantic-validator stack
  exhaustion while retaining IR arena stress validation.
- Preflight all model mass-assignment keys and value conversions before instance
  field writes, so rejected updates cannot partially mutate models.
- Fix `Model::replicate()` to omit generated timestamps and soft-delete state
  as well as the primary key; test replication of deleted records.

## 1.0.2 — 2026-10-21

- Added `gungnirc --verify-ir`, consistent profile/format diagnostics, output-error
  handling and cross-compiler diagnostic conformance checks.
- Fixed null-flow tracking across branches, loops and arrows; rejected unguarded
  nullable indexing and implicit instance-method captures before code generation.
- Fixed nullable model compound/postfix updates, immutable receiver enforcement
  and map-key iteration.
- Preserved healthy development-server processes when replacements fail and
  improved graceful Windows shutdown and POSIX startup-error reporting.
- Kept database pools responsive during slow connection validation.

See [Version 1.0.2 Installation](docs/release-v1.0.2.md).

## 1.0.1 — 2026-10-18

- Application SDK packages for Windows and Linux bundle SQLite persistence and
  OpenSSL password hashing alongside the existing core SDK.
- Current Spear templates, typed mail/notification delivery, shared cache
  coordination and external observability are included in the source release.
- Fixed cancellation/container cleanup deadlocks, logger sink mutation,
  coroutine ownership and executor startup cleanup.
- Updated Windows installer upgrade/PATH checks and downloadable workflow
  artifacts with checksums and source metadata.

See [Version 1.0.1 Installation](docs/release-v1.0.1.md).

## 1.0.0 — 2026-10-05

Gungnir 1.0.0 introduced a stable structured language and C++23 application
SDK, including:

- the Gungnir compiler, CLI and project generator;
- routing, controllers, requests, responses, middleware and validation;
- models, ORM, database and migration APIs;
- authentication, authorization, sessions and server-rendered views;
- jobs, events, scheduling, cache, storage, logging and native integration;
- Windows installation and Windows/Linux portable core SDK packages.

The published core SDK does not bundle every optional adapter.
See [Version 1.0.0 Installation](docs/release-v1.md) for package contents
and adapter requirements.
