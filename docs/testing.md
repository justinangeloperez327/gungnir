# Testing Gungnir Applications

Use `gungnirc --check` for syntax and semantic validation without C++ code
generation. Use `gungnirc --verify-ir` to additionally lower and verify the C++
intermediate representation without emitting C++ files. A full `gungnir build`
also generates and compiles native C++ executables.

## Check source

```sh
gungnirc app/controllers/HomeController.gnr --check
gungnirc --project . --verify-ir
gungnir build
```

Both checks support a single `.gnr` file or `--project <directory>`.
`--verify-ir` cannot be combined with `--check`, `--format`, `--compat`,
compiler dumps, or `-o`. It verifies compiler IR only: it does not emit C++,
invoke CMake, link native code, or start the application.

The structured compiler is the default. `--strict` is an explicit alias for
that mode, while `--compat` opts into the legacy transpiler for older source.
Do not combine `--strict` and `--compat` (either order is rejected). Invalid
structured source passed to `--format` reports the same diagnostic codes and
source locations as a compiler syntax check.

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
