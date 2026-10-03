# Gungnir Design Specifications

> **Gungnir 1.x design set.** These documents describe intended long-term architecture beyond the frozen `1.0.0-rc.1` release-candidate contract. They are not current usage instructions. The [current documentation index](../README.md) is authoritative for shipped behavior.

## 1.0 RC design governance

The 1.0 language, compiler, diagnostic, framework-semantic, and representative native C++ API contracts are frozen. Design proposals in this directory may target additive 1.x evolution or a future major version, but they must not be presented as already implemented unless the current guides and executable contract tests agree.

Design work should preserve the canonical compiler pipeline:

```text
source -> lexer -> parser -> syntax AST -> semantics -> validated AST -> typed C++ IR -> C++23 emitter
```

Runtime proposals must preserve the tested lifecycle, security, resilience, and API/ABI boundaries established for 1.0.

- [Application Lifecycle](application-lifecycle.md)
- [Abstract Syntax Tree](ast.md)
- [Async and Await](async.md)
- [Async Runtime](async-runtime.md)
- [Authentication](authentication.md)
- [Cache](cache.md)
- [CLI and Code Generation](cli-codegen.md)
- [Collections](collection.md)
- [Controllers](controller.md)
- [Database Runtime](database.md)
- [Dependency Injection](dependency-injection.md)
- [Errors](errors.md)
- [Events](event.md)
- [Expressions](expressions.md)
- [Packages and Extensions](extensions.md)
- [Functions](functions.md)
- [Grammar](grammar.md)
- [HTTP Runtime](http-runtime.md)
- [Language Frontend](language.md)
- [Language Types](language-types.md)
- [Listeners](listener.md)
- [Logging and Observability](logging-observability.md)
- [Mail](mail.md)
- [Middleware](middleware.md)
- [Migrations](migration.md)
- [Models](model.md)
- [Modules](modules.md)
- [MongoDB Adapter](mongodb.md)
- [MySQL Adapter](mysql.md)
- [Notifications](notification.md)
- [ORM](orm.md)
- [Policies and Authorization](policy.md)
- [PostgreSQL Adapter](postgresql.md)
- [Production and Deployment](production.md)
- [Queues and Jobs](queues.md)
- [ORM Relationships](relationships.md)
- [Requests](request.md)
- [Responses](response.md)
- [Routing](routing.md)
- [Scheduler](scheduler.md)
- [Security](security.md)
- [Semantic Analysis](semantics.md)
- [Sessions](session.md)
- [SQL Server Adapter](sqlserver.md)
- [Stability](stability.md)
- [Statements](statements.md)
- [Storage](storage.md)
- [Testing](testing.md)
- [Transpiler](transpiler.md)
- [Validated AST](validated-ast.md)
- [Validation](validation.md)
- [Views](view.md)
