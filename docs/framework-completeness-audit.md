# Framework Completeness Audit

Status: active pre-1.0 development audit  
Baseline: current `main` after the development-contract reset

This audit measures Gungnir against the intended expressive framework surface. It distinguishes native/runtime capability from first-class structured `.gnr` developer experience. A native C++ API does not make a feature complete when the intended Gungnir language surface is still missing.

## Classification

- **Implemented** — intended current surface exists end-to-end and has meaningful tests.
- **Partial** — substantial implementation exists, but important intended behavior or structured-language integration remains.
- **Missing** — intended first-class framework capability is not implemented.
- **Inconsistent** — implementation/documentation/tooling disagree or still describe obsolete development phases.

## Completeness matrix

| Area | Status | Current evidence | Completion work |
| --- | --- | --- | --- |
| Language frontend | Partial | Lexer, parser, structured syntax, semantic validation, validated AST and specialized lowerers exist. | Close the remaining target grammar/declaration/modifier gaps and remove dependence on compatibility/native forms for intended framework syntax. |
| Compiler pipeline | Implemented | Structured compiler reaches validated AST, typed C++ IR verification and C++ emission; check-only, robustness, determinism and conformance infrastructure exist. | Preserve as the canonical pipeline while framework syntax expands. |
| Models | Partial | Model fields, metadata, persistence integration, timestamps, soft deletes, casts and visibility metadata exist. | Complete intended metadata syntax, primary-key/fillable/cast ergonomics and relationship declarations in `.gnr`. |
| ORM/querying | Partial | Native query, persistence, pagination, soft deletes, eager loading and mutations exist. | Complete expressive `.gnr` query aliases/callbacks/lifecycle hooks and ensure the documented Laravel-like surface lowers consistently. |
| Relationships | Partial | Six native relationship wrappers, eager loading and pivot mutation exist. | Add first-class structured relationship declarations; then polymorphic and one-of-many behavior where retained by the design. |
| Database runtime | Implemented | Core connection/pool/transaction runtime plus SQLite, PostgreSQL, MySQL/MariaDB, SQL Server and MongoDB adapters exist behind build options. | Continue adapter parity/integration verification; do not redesign the core runtime. |
| Migrations | Implemented | Migration declarations, lowering, runner and CLI migration commands exist. | Fill only verified grammar/adapter parity gaps found by conformance tests. |
| HTTP runtime | Implemented | Request/response, cookies, server, security, streaming/WebSocket and production lifecycle infrastructure exist. | Preserve and extend through focused integration tests. |
| Routing | Partial | Route declarations, controller/action validation, middleware references, native groups/names/constraints/URL generation exist. | Add intended structured fluent modifiers, resource routes and implicit/explicit model binding. |
| Controllers | Partial | Typed sync/async actions, controller lowering, response helpers and injection plumbing exist. | Normalize structured action syntax and route-bound parameter behavior end-to-end. |
| Middleware | Implemented | Structured middleware contract, async continuation, aliases/groups/priority and semantic diagnostics exist. | Maintain while routing/bootstrap evolves. |
| Validation | Partial | Structured input, nested/wildcard fields and common/database rules exist. | Add UUID/date/file/image/conditional-required rules and custom-rule registration if retained in 1.0 scope. |
| Sessions | Implemented | Session lifecycle, flash state, middleware, memory and Redis stores exist. | Production concurrency semantics remain deployment-specific. |
| Authentication | Partial | Identity, guards, manager/context, session auth, password hashing and remember-token flow exist. | Add intended high-level `.gnr` auth ergonomics, user-model hydration/provider integration and production persistence conventions. |
| Authorization/policies | Partial | Structured policies, semantic validation and resource authorization exist. | Complete automatic authenticated-model resolution and decide whether async policy support belongs in 1.0. |
| Events/listeners | Implemented | Structured events/listeners and sync/async dispatch registration exist. | Distributed delivery belongs to queues/infrastructure rather than the core event contract. |
| Jobs/queues | Partial | Job declarations, payloads, workers, memory/Redis drivers, retries and after-commit dispatch exist. | Add first-class `.gnr` dispatch ergonomics and standard worker startup/application commands if retained. |
| Notifications | Partial | Structured notification contracts and native channels exist. | Supply built-in database/mail delivery adapters and recipient addressing conventions. |
| Mail | Partial | Message/Mailer, memory and SMTP transports plus structured mail composition exist. | Complete intended template/action composition and ergonomic send/queue integration. |
| Cache | Partial | Memory/Redis stores and repository operations exist. | Decide 1.0 typed serialization/tag/lock scope; current values are strings and remember is not single-flight. |
| Storage | Implemented | Local and optional S3-compatible disks plus manager and cancellation-aware operations exist. | Keep API names/documentation aligned; live endpoint behavior remains adapter-specific. |
| Scheduler | Partial | Interval/cron scheduling, timezone and distributed lock policies exist. | Add structured scheduling/bootstrap ergonomics and define standard runtime startup behavior. |
| Views/templates | Implemented | Escaping, raw output, loops, conditionals, partials, layouts, sections, components, helpers and bounded rendering exist. | Verify all documented constructs with generated application tests. |
| Dependency injection | Implemented | Container, generated factories, explicit bindings and injected structured artifacts exist. | Expand only as new framework artifacts require it. |
| Configuration/environment | Implemented | Environment and repository runtime exist and generated projects use editable bootstrap/configuration. | Verify documentation and production defaults. |
| Logging/observability | Implemented | Runtime observability infrastructure and optional OTLP exporter exist. | Keep production integration tests and documentation aligned. |
| CLI/project generation | Implemented | new/build/run/dev/lsp, generators and migration commands exist; PascalCase declaration filenames are documented. | Add commands only where a missing framework workflow justifies them. |
| Dev server | Implemented | Watch/rebuild/restart lifecycle with last-known-good process behavior exists. | Continue Windows/POSIX reliability testing. |
| LSP/formatter | Implemented | Diagnostics, symbols, definition, hover, completion, formatting and incremental document updates exist. | Expand language awareness as grammar grows. |
| Testing API | Implemented | HTTP dispatch/assertions, application helpers, compiler/runtime/ORM correctness, fuzzing and conformance infrastructure exist. | Add end-to-end tests for every newly completed structured framework surface. |
| Production runtime | Implemented | RuntimeHost, health, graceful shutdown, overload controls, retry and supervision exist. | Deployment-specific probes/TLS/reverse proxy choices remain application concerns. |
| Packaging/install | Implemented | CMake package, portable/install workflows and installed-consumer tests exist. | Maintain platform coverage through 1.0 preparation. |
| Documentation | Inconsistent | Current guides explicitly distinguish implementation from design, but several guides still contain obsolete numbered-phase language. | Remove phase-era wording and keep current guides tied to source/tests rather than historical roadmap phases. |

## Highest-priority completion groups

### Group 1 — Structured model and ORM surface

Complete the intended model declaration and ORM/query language before expanding peripheral features. This is the center of the Laravel-like developer experience.

Required outcomes:

- first-class model metadata syntax;
- primary key, table, connection, fillable/hidden/visible/casts consistency;
- structured relationships;
- eager-loading syntax and typed results;
- query callback/lambda support required by ORM operations;
- lifecycle hooks retained for 1.0;
- generated C++ and runtime API parity;
- compile-pass, compile-fail and generated execution coverage.

### Group 2 — Routing and binding

Required outcomes:

- structured route modifiers;
- groups/prefix/name/middleware syntax;
- parameter constraints;
- explicit and implicit model binding;
- resource routing if retained for 1.0;
- controller parameter validation and generated invocation parity.

### Group 3 — Authentication and authorization integration

Required outcomes:

- structured authentication ergonomics;
- provider/user-model integration;
- authenticated model hydration;
- session/remember-token conventions;
- policy actor/resource resolution;
- clear production persistence contract.

### Group 4 — Validation completeness

Required outcomes:

- UUID/date/file/image rules;
- conditional required rules;
- custom rule registration;
- upload/request integration;
- typed validation result behavior.

### Group 5 — Application services integration

Finish the first-class experience for queues/jobs, notifications, mail, cache and scheduler. Prefer coherent bootstrap/service APIs over static facades unless a facade materially improves the language.

### Group 6 — Documentation consistency

Remove numbered phase references from current guides. Historical phase/release information belongs only in changelog/migration history. Every current guide should describe capabilities by subsystem and test/contract name.

### Group 7 — Completeness gate

After Groups 1–6:

1. compile every documented `.gnr` example;
2. build generated C++ against installed framework packages;
3. execute framework integration fixtures;
4. verify all intended declarations and generators;
5. run database/backend capability matrices;
6. audit public headers and package consumption;
7. classify every remaining limitation as intentional 1.0 scope or blocker.

Only after this gate has no unintended Missing/Partial items should Gungnir enter compatibility freeze and release-candidate work.

## Immediate implementation order

Start with **Group 1: Structured model and ORM surface**. Do not spend another cycle on release numbering or compatibility freeze until the completeness gate is satisfied.
