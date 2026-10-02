# Database Runtime

> **Status: experimental, pre-1.0.** Phase 12 defines the current database behavioral-correctness baseline. Native C++ APIs and `.gnr` syntax remain subject to pre-1.0 compatibility rules.

## Current behavior

The runtime has named connections, a driver registry, connection pooling, query execution, transaction handling and backend SQL compilation. Register an optional adapter before calling `Application::configure_database()`.

Settings include connection name/backend, host/port, database, username/password, pool size, pool acquisition timeout, validation interval, reconnect attempts and backend options. The generated environment uses `DB_CONNECTION`, `DB_NAME`, `DB_POOL_SIZE`, `DB_HOST`, `DB_PORT`, `DB_DATABASE`, `DB_USERNAME` and `DB_PASSWORD`.

Use bindings for values; select backend-supported operations and inspect driver capabilities. Pool acquisition is cancellation-aware, and `Manager::pool_stats()` exposes primary-pool lease, availability, reconnect and timeout counters. Transaction scopes are strictly LIFO: a root transaction cannot be completed while a nested savepoint scope is active. Enable `GUNGNIR_WITH_SQLITE=ON` to build and automatically register SQLite. Use `DB_CONNECTION=sqlite` and a file in `DB_DATABASE`; relative paths resolve under the application root. `:memory:` requires a one-connection pool. SQLite supports prepared queries, cancellation, transactions and savepoints; schema changes needing a table rebuild and row-lock clauses are rejected explicitly. Decimal migration columns use TEXT to preserve exact values.

`model::Decimal` preserves SQL decimal text and scale; JSON/view serialization emits its exact string. Convert to binary floating point explicitly with `to_double()`. Integral model hydration rejects out-of-range values. PostgreSQL numeric, MySQL decimal, SQL Server decimal/numeric and MongoDB Decimal128 decoding retain decimal values.

## Limits and planned work

Blocking native clients are not made asynchronous by wrapping a caller in `async`. Cancellation remains limited by the selected native driver. Do not share one transaction scope concurrently across unrelated execution contexts, and do not assume all backends have equivalent savepoints, joins, DDL or decimal representations. Primary CI runs a live SQLite behavioral baseline for commit/rollback, savepoints, foreign keys, prepared bindings, nulls, exact decimal text and pool lease cleanup. See [Database and ORM Correctness](database-correctness.md) for the Phase 12 invariants and backend boundaries.

## Implementation references

- [include/gungnir/database/settings.hpp](../include/gungnir/database/settings.hpp)
- [include/gungnir/database/manager.hpp](../include/gungnir/database/manager.hpp)
- [include/gungnir/database/driver.hpp](../include/gungnir/database/driver.hpp)
- [include/gungnir/database/transaction.hpp](../include/gungnir/database/transaction.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/database.md).
