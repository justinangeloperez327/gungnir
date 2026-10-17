# Changelog

The changelog describes the capabilities available in Gungnir versions.
For installation and usage instructions, see the [README](README.md) and
[Documentation](docs/README.md).

## Unreleased

No unreleased changes.

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
