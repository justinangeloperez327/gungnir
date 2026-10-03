# Gungnir Documentation

Implementation baseline: active Gungnir development, reviewed on 3 October 2026.

Gungnir is under active feature-completion development. Language/compiler/diagnostic/native API contracts are not frozen for 1.0 yet. Start with the current usage guides below; future architecture remains under [design/](design/README.md).

| Documentation | Meaning |
| --- | --- |
| Current guides in this directory | APIs and implementation limits grounded in current source |
| `gnr` examples in current guides | Current frontend syntax; compile the generated C++ as well |
| `cpp` examples | Native interoperability; do not substitute their names into `.gnr` blindly |
| Design specifications | Intended contracts; examples may require future compiler/runtime work |

`gungnirc --check` is the authoritative structured semantic gate and intentionally stops before C++ IR/native compilation. Use `gungnir build` for backend verification. See [Stability](stability.md) and [Development Status](development-status.md) for the current compatibility boundary.

## Start here

- [Getting Started](getting-started.md)
- [Language Frontend](language.md)
- [CLI and Code Generation](cli-codegen.md)
- [Stability](stability.md)
- [Upgrading](upgrading.md)
- [Development Status](development-status.md)

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
- [Database and ORM Correctness](database-correctness.md)
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
- [Runtime Correctness](runtime-correctness.md)
- [Errors](errors.md)
- [Packages and Extensions](extensions.md)
- [Logging and Observability](logging-observability.md)
- [Production and Deployment](production.md)
- [Production Resilience](production-resilience.md)
- [Performance Baseline](performance.md)
- [Native API and ABI Stability](native-api-abi.md)
- [Security](security.md)
- [Security Hardening](security-hardening.md)
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
- [Compiler Fuzzing](fuzzing.md)
- [Compiler Conformance](compiler-conformance.md)
- [Transpiler](transpiler.md)
- [Compiler profiles and feature status](compiler-profiles.md)
- [Framework Semantic Contracts](framework-semantics.md)

[Editor tooling](editor-tooling.md) covers the structured language server and formatter.
