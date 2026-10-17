# Gungnir Documentation

Gungnir is an expressive web framework built with C++23. Applications use
`.gnr` files for models, controllers, routes, middleware and other application
logic. Gungnir validates the source and builds native executables.

Start with [Getting Started](getting-started.md) to install the SDK, create a
project, build it and run the local server.

## Setup and development

- [Getting Started](getting-started.md) — install, create, run and troubleshoot
- [SDK Packages](sdk-packages.md) — choose the required native adapters
- [CLI and Code Generation](cli-codegen.md) — scaffold application files and run commands
- [Configuration](configuration.md) — application and environment settings
- [Editor Tooling](editor-tooling.md) — language server integration
- [Testing](testing.md) — check code and test applications

## HTTP applications

- [Routing](routing.md) — endpoints, route names and model binding
- [Controllers](controller.md) — request handlers
- [Middleware](middleware.md) — request processing
- [Requests](request.md) — inspect input
- [Responses](response.md) — send JSON, text and files
- [Validation](validation.md) — validate application data
- [Views and Spear](view.md) — render server-side HTML
- [Authentication](authentication.md) — sessions and identity
- [Policies and Authorization](policy.md) — protect resources
- [Sessions](session.md) — store user session state
- [Security](security.md) — configure HTTP security
- [Security Hardening](security-hardening.md) — security behaviors and limits

## Databases and models

- [Database](database.md) — connections and transactions
- [Models](model.md) — model declarations and persistence
- [ORM](orm.md) — querying and updating records
- [Relationships](relationships.md) — associated records and eager loading
- [Collections](collection.md) — manipulate application collections
- [Migrations](migration.md) — create and change schemas
- [MySQL](mysql.md) — MySQL and MariaDB
- [PostgreSQL](postgresql.md) — PostgreSQL
- [SQL Server](sqlserver.md) — SQL Server
- [MongoDB](mongodb.md) — MongoDB

## Application services

- [Dependency Injection](dependency-injection.md) — resolve dependencies
- [Application Lifecycle](application-lifecycle.md) — startup and shutdown
- [Events](event.md) — dispatch events
- [Listeners](listener.md) — handle events
- [Queues and Jobs](queues.md) — run background work
- [Scheduler](scheduler.md) — recurring tasks
- [Notifications](notification.md) — notify recipients
- [Mail](mail.md) — compose and send email
- [Cache](cache.md) — cache values and use locks
- [Distributed Coordination](cache-coordination.md) — share cache and quotas through Redis
- [Storage](storage.md) — read and write files
- [Logging and Observability](logging-observability.md) — application diagnostics
- [External Observability](external-observability.md) — export telemetry
- [Packages and Extensions](extensions.md) — extend the application

## Gungnir language

- [Language Overview](language.md) — basic syntax and declarations
- [Language Types](language-types.md) — available types
- [Expressions](expressions.md) — values and operators
- [Statements](statements.md) — branching and looping
- [Functions](functions.md) — declare and call functions
- [Async and Await](async.md) — asynchronous application code
- [Modules](modules.md) — imports and code organization
- [Grammar](grammar.md) — language syntax reference
- [Errors and Diagnostics](errors.md) — understand errors

## Runtime and deployment

- [HTTP Runtime](http-runtime.md) — configure HTTP serving
- [Async Runtime](async-runtime.md) — native tasks and cancellation
- [Production and Deployment](production.md) — run production applications
- [Linux Deployment](production-deployment.md) — Nginx, Supervisor and persistent services
- [Production Resilience](production-resilience.md) — health and capacity limits
- [Application Performance](application-performance.md) — run the load measurement tool
- [Native API and ABI](native-api-abi.md) — native compatibility
- [Stability](stability.md) — compatibility and version contracts
- [Upgrading](upgrading.md) — rebuild applications with a different SDK
- [Version 1.0.1 Installation](release-v1.0.1.md) — current binary downloads and their scope
- [Version 1.0.0 Installation](release-v1.md) — older core-only binary downloads

## Working examples

- [Hello Gungnir](../examples/hello/README.md) — minimal HTTP project
- [Spear](../examples/spear/README.md) — server-rendered views
- [Linux Application](../examples/production-deployment/README.md) — deployment walkthrough

**SDK note:** Version 1.0.1 provides core and application SDKs. Choose the
application profile for bundled SQLite and password hashing; see
[SDK Packages](sdk-packages.md) for other adapters and source builds.
