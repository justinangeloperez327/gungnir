# Changelog

This changelog records published releases and material development changes. Git history remains the detailed implementation record.

## Unreleased

- Link the optional OTLP adapter in generated applications and continue valid
  HTTP traceparent identities through logical request and persisted queue context.
- Refresh boot services and activate generated workers/migrations after boot to
  retain tracing/meter bindings for lifecycle, processing and database work.
- Bound OTLP records, pending bytes, responses, retries, flush and shutdown;
  report rejected/lost records, preserve independent trace/metric delivery and
  handle collector partial success without leaking responses into stdout.
- Add concurrent bounded JSON stream logging and verify installed HTTP/SQLite/
  Redis worker correlation, redaction, errors, exemplars and outage isolation
  through the checksum-pinned official OpenTelemetry Collector.

- Add owned canonical cache leases and explicitly coordinated cache factories,
  with Redis owner checks, exception cleanup and literal namespace isolation.
- Add an explicit atomic Redis HTTP rate-limit backend and verify installed
  applications across concurrent processes, holder crashes and process restarts.

- Add an application SDK profile with bundled static SQLite and OpenSSL Crypto,
  relocatable CMake targets, dependency notices and installed capability metadata.
- Build core/application portable packages and Windows installers; exercise
  migrations, stored passwords, login/logout and process restart using relocated SDKs.
- Report missing password support before generated application compilation and
  provide actionable database adapter installation guidance.

- Add injected typed mail and notification delivery, owned recipient envelopes,
  binary attachments, model/channel routing and versioned queued snapshots.
- Resolve queued transports/channels in fresh worker application scopes; verify
  Redis restart/retry delivery through loopback SMTP, SQLite and custom channels.
- Link optional SMTP in generated applications and keep post-release main
  development from republishing an existing version tag.

Future changes will be recorded here.

## 1.0.0 — 2026-10-05

- published the merged application framework with Windows setup, Windows/Linux portable SDKs and SHA-256 checksums;
- assigned package 1.0.0, language/compiler/diagnostic/native API contract 1.0 and native ABI epoch 1;
- froze the existing structured contract while retaining compiler/platform/security/package regression gates;
- documented the packaged core, optional adapter dependencies and pending generated mail/notification delivery integration;
- verified installed project creation/build/run and retained the canonical application acceptance suites.

The application and compiler changes below summarize the work included in this release.

### Application services

- connected typed Config, Logger, Telemetry and Span APIs to native application services;
- preserved request scopes in generated factories and created a fresh scope for each injected job execution;
- resolved model policy actors through the owning ORM while preserving explicit identity mappings;
- added persistent Redis remember tokens with atomic consumption, expiry and revocation;
- verified installed views, hidden model fields, bound authorization and shared authentication across independent server processes;
- isolated application tracing/metrics exporters, added facade redaction, and connected generated HTTP shutdown to cancellation and exporter cleanup;
- documented and validated the public application-service examples.

### Historical development consistency reset

- removed the premature 1.0 release-candidate positioning;
- returned package/language/compiler/diagnostic/native API metadata to development status;
- returned the native ABI epoch to development epoch 0;
- reopened the structured feature set for planned completion work;
- retained compiler correctness, conformance, security, runtime, performance, package and documentation gates;
- replaced RC promotion logic with a framework completeness rule for the eventual 1.0 release.

### Completed maturity work retained

The release includes:

- compiler correctness and canonical structured compilation;
- typed C++ IR;
- semantic/type-system hardening;
- authoritative semantic checking;
- diagnostic/source mapping hardening;
- fuzzing and cross-compiler conformance;
- runtime lifecycle correctness;
- database/ORM correctness;
- framework semantic contracts;
- security hardening;
- production resilience;
- performance benchmarks;
- native package/API/ABI verification infrastructure;
- documentation and ecosystem integrity gates.

These capabilities remain implemented, but they are not treated as a final 1.0 compatibility promise while feature completion continues.

## [0.9.0] - 2026-10-02

### Compiler stabilization

- established the structured compiler as the canonical compilation path;
- established dedicated structural C++ IR boundaries;
- completed semantic/type-system validation for the then-current profile;
- made `gungnirc --check` authoritative through validated semantics;
- hardened diagnostics/source mapping;
- added fuzzing and compiler robustness gates;
- added GCC, Clang and MSVC conformance with deterministic structured output.

### Runtime and framework maturity

- hardened request/coroutine lifecycle and graceful shutdown;
- strengthened database and ORM correctness;
- hardened framework semantics and security boundaries;
- added production resilience, health, retry and overload controls;
- established reproducible compiler, HTTP/routing and ORM benchmarks;
- introduced native API/ABI verification infrastructure.

### Packaging

- Linux x86_64 portable package;
- Windows x86_64 portable package;
- Windows setup executable;
- installed CMake package targets;
- compiler and CLI contract/version verification.
