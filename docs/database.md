# Database

Gungnir's database layer is the execution foundation for the ORM, migrations, and database-aware validation.

## Connections

Applications configure named connections through `database::Manager`. A connection owns a concrete driver and exposes parameterized execution.

```cpp
auto result = connection->execute(
    "SELECT * FROM users WHERE email = ?",
    {email}
);
```

Bindings are passed separately from the statement. Application code should not interpolate untrusted values into SQL.

A `database::Query` can also carry a statement and its bindings as one value.

## Drivers

Gungnir separates the database contract from vendor adapters. `Driver` is the adapter boundary and `DriverRegistry` registers concrete drivers.

The core recognizes PostgreSQL, MySQL, SQL Server, and MongoDB configuration, but recognizing a backend is not a claim that a production driver is bundled. A backend is usable only when its concrete adapter is registered.

MongoDB remains a document backend and should not be forced through relational SQL semantics.

## Pooling

`ConnectionPool` creates a bounded set of connections and distributes acquisitions across them. The current pool is deliberately small and deterministic; production queueing and lease semantics belong to later runtime work rather than being hidden behind a misleading API.

## Transactions

`Manager::transaction()` pins work to one connection. `Transaction::run()` commits on success and rolls back on exceptions. Nested transaction/savepoint semantics are explicit driver capabilities and are not emulated by the core.

## Errors

Driver execution failures are wrapped as `database::Error` with backend, connection name, and statement context. Binding values are intentionally not copied into the exception to reduce accidental credential or personal-data leakage.

## Health

`Connection::healthy()` delegates to the driver ping contract. This is a connectivity signal, not an application readiness guarantee.
