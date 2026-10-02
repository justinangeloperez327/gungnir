# Gungnir Documentation

Implementation baseline: source commit `83c29209a83e2696f40d7db22e3eba618cb77cb4`, reviewed on 2 October 2026.

Gungnir is experimental. Start with the current usage guides below. The full proposed language and architecture contracts are preserved under [design/](design/README.md).

| Documentation | Meaning |
| --- | --- |
| Current guides in this directory | APIs and implementation limits grounded in current source |
| `gnr` examples in current guides | Current frontend syntax; compile the generated C++ as well |
| `cpp` examples | Native interoperability; do not substitute their names into `.gnr` blindly |
| Design specifications | Intended contracts; examples may require future compiler/runtime work |

`gungnirc --check` is not a complete native type check. The walkthrough explains how to verify generated source. Pin a revision for application development.

## Start here

- [Getting Started](getting-started.md)
- [Language Frontend](language.md)
- [CLI and Code Generation](cli-codegen.md)
- [Stability](stability.md)

## HTTP application

- [Routing](routing.md)
- [Controllers](controller.md)
- [Middleware](middleware.md)
- [Requests](request.md)
- [Responses](response.md)
- [Validation](validation.md)
- [Views](view.md)
- [Authentication](authentication.md)
- [Policies and Authorization](policy.md)

## Data

- [Models](model.md)
- [ORM](orm.md)
- [Collections](collection.md)
- [ORM Relationships](relationships.md)
- [Migrations](migration.md)
- [Database Runtime](database.md)
- [MySQL Adapter](mysql.md)
- [PostgreSQL Adapter](postgresql.md)
- [SQL Server Adapter](sqlserver.md)
- [MongoDB Adapter](mongodb.md)

## Services

- [Events](event.md)
- [Listeners](listener.md)
- [Notifications](notification.md)
- [Mail](mail.md)
- [Cache](cache.md)
- [Sessions](session.md)
- [Queues and Jobs](queues.md)
- [Scheduler](scheduler.md)
- [Storage](storage.md)

## Runtime and operations

- [Application Lifecycle](application-lifecycle.md)
- [Dependency Injection](dependency-injection.md)
- [Async Runtime](async-runtime.md)
- [HTTP Runtime](http-runtime.md)
- [Errors](errors.md)
- [Packages and Extensions](extensions.md)
- [Logging and Observability](logging-observability.md)
- [Production and Deployment](production.md)
- [Security](security.md)
- [Testing](testing.md)

## Compiler implementation

- [Language Types](language-types.md)
- [Expressions](expressions.md)
- [Statements](statements.md)
- [Functions](functions.md)
- [Async and Await](async.md)
- [Modules](modules.md)
- [Grammar](grammar.md)
- [Abstract Syntax Tree](ast.md)
- [Semantic Analysis](semantics.md)
- [Validated AST](validated-ast.md)
- [C++ Intermediate Representation](cpp-ir.md)
- [Compiler Correctness](compiler-correctness.md)
- [Compiler Conformance](compiler-conformance.md)
- [Transpiler](transpiler.md)
- [Compiler profiles and feature status](compiler-profiles.md)

[Editor tooling](editor-tooling.md) covers the structured language server and formatter.
