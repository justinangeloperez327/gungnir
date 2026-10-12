# Gungnir Documentation

Gungnir is an expressive web application framework built in C++23. It combines a concise, framework-oriented `.gnr` language with a native C++ runtime so applications can use high-level conventions without giving up compiled performance, static analysis, or native interoperability.

These guides focus on the developer-facing application language and runtime.
Compiler internals and historical audits are separated into contributor references.

**Choose an SDK first:** the published 5 October 2026 v1.0.0 installers
contain the core SDK; current `main` includes subsequently merged,
unreleased functionality. Read [Release and Source Status](development-status.md)
and [Version 1.0.0](release-v1.md) before using newer examples.

## Read by task

- **First HTTP app:** [Getting Started](getting-started.md) →
  [Routing](routing.md) → [Controllers](controller.md) →
  [Requests](request.md) and [Responses](response.md).
- **Database app:** [SDK Packages](sdk-packages.md) →
  [Database](database.md) → [Models](model.md) →
  [ORM](orm.md) → [Migrations](migration.md).
- **HTML pages on current source:** [Spear Views](view.md) →
  [Authentication](authentication.md) → [Policies](policy.md).
- **Production services:** [Queues](queues.md) and [Scheduler](scheduler.md) →
  [Production](production.md) →
  [Linux deployment reference](production-deployment.md).

Runnable examples: [Hello Gungnir](../examples/hello/README.md) for minimal
HTTP and [Spear](../examples/spear/README.md) for current-source views.

## Getting started

- [Getting Started](getting-started.md)
- [SDK Packages](sdk-packages.md)
- [Language](language.md)
- [CLI and Code Generation](cli-codegen.md)
- [Editor Tooling](editor-tooling.md)
- [Testing](testing.md)
- [Production and Deployment](production.md)
- [Linux Deployment Reference](production-deployment.md)

## HTTP applications

- [Routing](routing.md)
- [Controllers](controller.md)
- [Middleware](middleware.md)
- [Requests](request.md)
- [Responses](response.md)
- [Validation](validation.md)
- [Views and Spear](view.md)
- [Authentication](authentication.md)
- [Policies and Authorization](policy.md)
- [Security](security.md)

## Data

- [Models](model.md)
- [ORM](orm.md)
- [Collections](collection.md)
- [Relationships](relationships.md)
- [Migrations](migration.md)
- [Database](database.md)
- [MySQL](mysql.md)
- [PostgreSQL](postgresql.md)
- [SQL Server](sqlserver.md)
- [MongoDB](mongodb.md)

## Application services

- [Dependency Injection](dependency-injection.md)
- [Configuration](configuration.md)
- [Application Lifecycle](application-lifecycle.md)
- [Sessions](session.md)
- [Events](event.md)
- [Listeners](listener.md)
- [Queues and Jobs](queues.md)
- [Notifications](notification.md)
- [Mail](mail.md)
- [Cache](cache.md)
- [Distributed coordination](cache-coordination.md)
- [Storage](storage.md)
- [Scheduler](scheduler.md)
- [Logging and Observability](logging-observability.md)
- [External Observability](external-observability.md)
- [Packages and Extensions](extensions.md)

## The Gungnir language

- [Language Types](language-types.md)
- [Expressions](expressions.md)
- [Statements](statements.md)
- [Functions](functions.md)
- [Async and Await](async.md)
- [Modules](modules.md)
- [Grammar](grammar.md)
- [Errors and Diagnostics](errors.md)

## Runtime and native integration

- [Async Runtime](async-runtime.md)
- [HTTP Runtime](http-runtime.md)
- [Native API and ABI](native-api-abi.md)
- [Performance](performance.md)
- [Application Load and Performance](application-performance.md)
- [Application Load Baseline](application-performance-baseline.md)
- [Production Resilience](production-resilience.md)
- [Security Hardening](security-hardening.md)

## Compatibility and upgrades

- [Stability](stability.md)
- [Upgrading](upgrading.md)

## Compiler and contributor reference

These documents explain how Gungnir itself works. Application developers do not need them for ordinary framework use.

- [Abstract Syntax Tree](ast.md)
- [Semantic Analysis](semantics.md)
- [Validated AST](validated-ast.md)
- [C++ Intermediate Representation](cpp-ir.md)
- [Transpiler](transpiler.md)
- [Compiler Correctness](compiler-correctness.md)
- [Compiler Conformance](compiler-conformance.md)
- [Compiler Fuzzing](fuzzing.md)
- [Compiler Profiles](compiler-profiles.md)
- [Framework Semantic Contracts](framework-semantics.md)
- [Runtime Correctness](runtime-correctness.md)
- [Database and ORM Correctness](database-correctness.md)

- [Development Status](development-status.md)
- [Framework Completeness Audit](framework-completeness-audit.md)
- [Request Context Contract Audit](../engineering/request-context-contract-audit.md)

- [Authentication Contract Audit](../engineering/authentication-contract-audit.md)
- [Cache Coordination Contract Audit](../engineering/cache-coordination-contract-audit.md)

- [Version 1.0.0 release and installation](release-v1.md)
