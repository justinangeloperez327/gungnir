# Database and ORM Correctness

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

## Correctness contract

A database operation must complete, fail, or cancel without leaking a pool lease or leaving transaction ownership ambiguous. ORM operations must preserve model state consistently with the observable database result.

The Phase 12 gate covers the following invariants.

### Connection pools

- every successful acquisition owns exactly one lease;
- destroying the returned shared connection releases that lease exactly once;
- cancellation while waiting for a lease raises `OperationCancelled` and is not counted as pool exhaustion;
- a real acquisition deadline increments `acquire_timeouts`;
- failed idle validation may recreate the pool entry according to `reconnect_attempts`;
- `Manager::pool_stats()` exposes the primary pool's lease, availability, reconnect, and timeout counters for correctness checks and operational inspection.

### Transactions

Transaction scopes are strictly LIFO.

A root transaction cannot commit or roll back while a nested savepoint scope is still active. Every scope has an internal depth and ownership identifier; stale or out-of-order scope completion is rejected instead of silently changing a different transaction level.

Nested commit behavior is backend-aware:

- PostgreSQL, MySQL, and SQLite release the active savepoint;
- SQL Server retains its native savepoint semantics;
- MongoDB does not emulate SQL savepoints.

Nested rollback discards callbacks registered at that nested level. Nested commit promotes them to the parent level. Root commit invokes accumulated `after_commit` callbacks only after the database commit succeeds.

If an `after_commit` callback throws, the transaction remains committed. The `Transaction` object is marked inactive before the callback failure is rethrown.

### Cancellation and errors

Database cancellation remains cooperative with the selected driver. `Connection::execute(..., CancellationToken)` registers the driver's cancellation hook and reports cancellation as `OperationCancelled`.

Non-Gungnir driver exceptions are normalized to `database::Error` with backend, connection name, and statement context. A driver that already throws `database::Error` retains that error.

### Hydration and persistence

Model hydration uses declared generated metadata and synchronizes original values so freshly hydrated models are clean.

Conversions reject unsafe values rather than truncating them:

- signed/unsigned integer range violations throw;
- negative values cannot hydrate unsigned fields;
- invalid type conversions throw;
- nullable fields preserve SQL null;
- `model::Decimal` retains exact decimal text.

Persistence follows observable database results:

- successful inserts mark the model persisted and synchronize original values;
- generated primary keys are applied when the backend returns an inserted ID;
- successful updates synchronize dirty values;
- a zero-row update returns false and leaves dirty state intact;
- fillable rules still apply to mass assignment;
- timestamps and soft-delete behavior remain part of the ORM regression gate.

### Eager loading and N+1 baseline

Eager loading batches relation keys before loading related rows. For a normal `hasMany`, `hasOne`, or `belongsTo` eager load, parent count does not change the number of relation queries.

The Phase 12 regression gate loads multiple parents with a one-slot pool and requires:

1. one query for the parent collection;
2. one batched query for the relation;
3. correct relation attachment for each parent;
4. zero leased connections after completion.

Nested and pivot relationships keep their backend-appropriate multi-query behavior, but must not silently fall back to one query per parent.

## SQLite baseline

Primary CI enables SQLite and runs a live behavioral baseline against an in-memory database. It verifies:

- commit and rollback;
- nested savepoint rollback;
- foreign-key enforcement;
- prepared bindings;
- SQL null preservation;
- exact decimal text preservation;
- zero leaked pool leases.

PostgreSQL and MySQL retain separate live adapter jobs. SQL Server and MongoDB behavior is tested through their adapter/integration configuration when those optional jobs are enabled. Backend differences are explicit rather than normalized into semantics the backend cannot guarantee.

## Current limits

Phase 12 is a correctness baseline, not a claim that blocking native database clients have become asynchronous. Cancellation can only interrupt operations that the underlying driver can cancel.

Transaction scopes should not be shared concurrently across unrelated execution contexts. A transaction retains its leased connection for the scope lifetime.

The generated model surface remains 1.0 RC. Compiler-level framework semantics that are still marked Partial are handled separately from runtime ORM correctness.

## Implementation references

- [include/gungnir/database/connection.hpp](../include/gungnir/database/connection.hpp)
- [include/gungnir/database/pool.hpp](../include/gungnir/database/pool.hpp)
- [include/gungnir/database/transaction.hpp](../include/gungnir/database/transaction.hpp)
- [include/gungnir/database/manager.hpp](../include/gungnir/database/manager.hpp)
- [include/gungnir/model/model.hpp](../include/gungnir/model/model.hpp)
- [include/gungnir/orm/executor.hpp](../include/gungnir/orm/executor.hpp)
- [tests/database_correctness.cpp](../tests/database_correctness.cpp)
- [tests/orm_correctness.cpp](../tests/orm_correctness.cpp)
- [tests/sqlite_correctness.cpp](../tests/sqlite_correctness.cpp)

See [Database Runtime](database.md), [Models](model.md), and [Testing](testing.md).
