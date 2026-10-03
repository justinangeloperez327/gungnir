# Product contract audit

Baseline: `8033cf6837f9cdc55ab5dfb5f805076f015dde67` (merged PRs #173 and #174).
This is an engineering record; public guides remain the product specification.

## Evidence and classification

**Complete** requires documented behavior through the canonical language, generated
native compilation and execution. **Partial** denotes a present implementation
with an unclosed integration or verification gap. **Missing**, **Inconsistent**,
**Legacy-only** and **Documentation conflict** identify the nature of a proven gap.
Adapter coverage is not inferred from a header or an optional build flag.

| Documented subsystem | Baseline classification | Inspectable evidence and gap |
| --- | --- | --- |
| Compiler pipeline, IR, diagnostics | Complete for the existing accepted language | `src/language/{syntax,validated,cpp_ir,emitter}.cpp`; baseline `compiler_correctness`, `framework_semantics` and `structured_generated` pass. New framework contracts require their own evidence. |
| Language types, functions, statements, expressions, async | Partial / Documentation conflict | Canonical typed validation and generated tests exist; several guides still describe the compatibility parser or historical target limitations. ORM fluent return lifetimes need native execution coverage. |
| Modules | Complete for the documented module graph | `compiler.cpp`, `validated.cpp`, `cpp_ir.cpp`, `structured_language.cpp`, generated multi-module fixture. Imports and output are deterministic; new cross-module relationship resolution is unverified at baseline. |
| Models and serialization | Partial | Structured metadata is already canonical (#173). Relationship storage/metadata is absent; generated ORM functions omit complete ORM headers. Casted attribute persistence needs generated execution coverage. |
| ORM and collections | Inconsistent | Documented `orderBy("name", "desc")` passes validation but emitted C++ cannot build. Static and query terminal methods have mismatched availability/results. Native query/collection implementations are retained. |
| Relationships | Legacy-only language syntax / Partial subsystem | `hasMany<Post>()` fails `SyntaxParser` with GNR2001. Native six-kind metadata, eager loading, scoped queries and pivot mutations exist in `model/relation.hpp` and `orm/{executor,relation_query,relation_mutation}.hpp`. |
| Database and backend adapters | Partial verification | Native manager/pools, transactions, cancellation and six adapters exist. SQLite and generated application tests must run; live PostgreSQL/MySQL/SQL Server/MongoDB matrices are separate required evidence. |
| Migrations | Partial verification | Structured `Table` callback lowering and migration runner exist. Preserve them and execute generated migrations against SQLite; backend-specific DDL remains a separate matrix. |
| Routing and model binding | Partial | CLI route assembly and native groups/names/constraints/bindings exist. Canonical routes are handled outside ordinary modules; documented fluent groups and typed model actions require application-level checks. |
| Controllers | Partial | Logical Response, sync/async actions and injection exist. Bound models and all documented response helpers are not proved by the existing hello fixture. |
| Requests and responses | Partial | Native APIs exist, but `validated.cpp` exposes only a subset: documented `request.json()`, sessions and response headers need canonical-call coverage. |
| Middleware | Partial | Structured signatures and async forwarding are tested. The documented `request.authenticated()` call is absent from the canonical builtin surface. |
| Validation | Partial | Structured/nested input and database rules exist. UUID/date/upload/image/conditional/custom-rule contracts require code and regression checks. |
| Authentication | Partial | Native guard/session/password/remember-token components exist. `auth.attempt` and typed `request.user()` are not canonical prelude calls; request-to-provider/model flow needs integration proof. |
| Policies and authorization | Partial | Structured policies and `authorize` are tested. Automatic actor hydration and bound resource resolution require request-level evidence. |
| Events and listeners | Partial verification | Structured immutable events, registration and sync/async dispatch are exercised in `structured_generated.cpp`. Declaration injection/priority and application dispatch remain acceptance requirements. |
| Jobs and queues | Partial verification | Structured payload reconstruction, worker registration, memory/Redis drivers and after-commit support exist. Complete `.gnr` dispatch/startup and durable retry workflows need an acceptance application. |
| Notifications and mail | Partial verification | Structured composition, `core/services.hpp`, builtin channel adapters and memory/SMTP transports exist. Recipient addressing and queued delivery need generated application coverage; do not reimplement existing adapters. |
| Sessions | Partial language integration | Native lifecycle, rotation, flash and memory/Redis stores are tested. Documented chained session calls are absent from canonical semantic builtins. |
| Cache | Partial | Native memory/Redis repository supports string values; documented service variables, typed value serialization and distributed locks need canonical integration. |
| Storage | Partial language integration | Native local/S3 disks, safe paths and cancellation exist. Documented `.put`/`.get` service access needs typed integration and upload tests. |
| Scheduler | Partial language integration | Native intervals/cron/timezones/locks and RuntimeHost exist. `.gnr` application scheduling and worker startup are not proved. |
| Views | Partial verification / Documentation conflict | Escaping, layouts/components and rendering limits have native tests. `view.md` promises model visibility while `security.md` disputes it; verify conversion and correct that conflict. |
| Dependency injection and lifecycle | Partial verification / Documentation conflict | Native container/request scopes and generated factories exist. The guides retain implementation-status wording; coroutine ownership must remain covered when adding service calls. |
| Configuration and environment | Partial verification | Native configuration and generated editable bootstrap exist. Typed service configuration and secret handling require acceptance coverage. |
| Logging and observability | Partial verification | Native sinks, metrics/traces and optional OTLP exist. Application configuration and required framework paths need explicit tests. |
| Security and production runtime | Partial verification | Existing HTTP framing/trust, session/CSRF/view and lifecycle regression gates are retained. New ORM/mass-assignment/relationship paths need focused regressions; no whole-product security claim. |
| CLI, generators, dev server | Partial verification / Documentation conflict | Structured generation and installed-app tests exist. Getting-started examples omit required listener/policy/notification type arguments. Every generator must build against the installed package. |
| LSP and formatter | Partial verification | Structured snapshots and tooling tests exist. New relationship symbols/navigation/completion require regression coverage. |
| Testing and native packaging | Partial completeness evidence | Installed consumers and compiler/platform workflows exist. Existing fixtures do not constitute the required full application acceptance test. |
| Performance | Partial completeness evidence | Compiler/HTTP/ORM benchmark suites exist. Query lifetimes and batched eager loading are correctness requirements before measurement. |
| Documentation and release automation | Inconsistent | Documentation job fails on four missing index entries; release workflow is malformed YAML at baseline. Product examples and contributor/audit navigation must be checked without version changes. |

## First implementation contract

Preserve model metadata and native ORM infrastructure. Add all six canonical
relationship declarations, typed key resolution and generated relation metadata;
reconcile documented queries with runtime methods; prove migrations, hydration,
persistence, eager loading, loaded state, scoped queries and pivot operations in a
generated SQLite application. Add compile-fail and module-resolution regressions.

The full framework remains below the 1.0 completeness gate until the other
integration gaps and required adapter/platform matrices have independent evidence.

## Verified implementation batch

The canonical compiler now represents all six relationship kinds separately from
persistence attributes and emits their native relation metadata. Key inference,
explicit/named keys, imported aliases, custom string primary keys, string pivot
keys and self relationships have regression coverage. Invalid declarations and
calls produce deterministic diagnostics in emission and validation-only modes.

`tests/fixtures/structured/orm.gnr` passes parsing, validation, IR verification,
C++23 compilation and SQLite execution. Its generated migrations and controller
exercise creation, hydration, JSON casts and hidden serialization, dirty tracking,
soft deletion/restoration, optional/required retrieval, pagination, collection
callbacks, all six eager-load kinds, nested loading, scoped queries and pivot
cache invalidation. The specified eager-load graph uses ten reported queries for
three parents, including the pivot query; loaded traversal adds none. A one-slot
pool verifies that eager loading releases ordinary query leases.

Local GCC 13 Debug verification: **86/86 CTest tests pass**. The same generated
ORM contract and SQLite executable build and run against an installed package.
Installed CLI generators, migrations, live HTTP, incremental builds and dev
restart/error handling pass. **26 public model, ORM, relationship and collection
examples pass strict validation**. Documentation/development integrity checks and
workflow YAML parsing pass. Compiler conformance now compares ORM C++, validated
AST and IR output across GCC, Clang and MSVC; inspect the associated PR checks for
the remote compiler/platform results.

This closes the relationship declaration and generated ORM execution gaps in this
batch. It does not establish live adapter equivalence or close the other language
and application-service integration gaps in the baseline matrix.
