# Testing Gungnir Applications

Use `gungnirc --check` to catch language and framework errors before compiling
an application. A full build additionally verifies the generated native C++.

## Check source

```sh
gungnirc app/controllers/HomeController.gnr --check
gungnir build
```

A successful source check does not prove a database connection, an HTTP
listener or an external adapter is configured correctly. Start the application
and exercise the real endpoints for those checks.

## Test HTTP endpoints

Gungnir provides native `testing::Http` helpers that dispatch through the
real router. The harness supports `get`, `post` and `send` for additional
HTTP methods, along with response assertions. Use it to verify route
parameters, middleware, response status and bodies without running a public
network listener.

See [Routes](routing.md), [Controllers](controller.md) and
[Responses](response.md).

## Test services and persistence

Use memory-backed transports, stores and log sinks to test behavior without
sending external mail or connecting to production services. For database tests,
use a dedicated test database and apply migrations before running queries.
Backends such as PostgreSQL, MySQL, SQL Server, MongoDB, Redis and SMTP
require the appropriate compiled adapter and a configured service.

Run an installed application's tests according to its generated build or
project-specific test commands. When building the framework from source,
configure the test targets and use CTest:

```sh
cmake -S . -B build-tests -DGUNGNIR_BUILD_TESTS=ON
cmake --build build-tests --parallel 2
ctest --test-dir build-tests --output-on-failure
```

## What to verify before deployment

Check input validation, authentication, policy rules, session persistence,
database commits and rollback, queue/worker operation, error responses,
health/readiness, and shutdown handling for the adapters your application
actually uses. See [Security](security.md),
[Database](database.md) and [Production](production.md).
