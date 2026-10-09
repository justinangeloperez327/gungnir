# Gungnir Documentation

Gungnir is an expressive web application framework built in C++23. It combines a concise, framework-oriented `.gnr` language with a native C++ runtime so applications can use high-level conventions without giving up compiled performance, static analysis, or native interoperability.

The documentation defines the Gungnir developer experience: how applications are structured, how the language works, and how framework services are used.

## Getting started

- [Getting Started](getting-started.md)
- [SDK Packages](sdk-packages.md)
- [Language](language.md)
- [CLI and Code Generation](cli-codegen.md)
- [Editor Tooling](editor-tooling.md)
- [Testing](testing.md)
- [Production and Deployment](production.md)

## HTTP applications

- [Routing](routing.md)
- [Controllers](controller.md)
- [Middleware](middleware.md)
- [Requests](request.md)
- [Responses](response.md)
- [Validation](validation.md)
- [Views](view.md)
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

- [Version 1.0.0 release and installation](release-v1.md)
