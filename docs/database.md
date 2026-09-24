# Database

Gungnir's database layer is the execution foundation for the ORM, migrations, and database-aware validation.

## Connections

Applications configure named connections through `database::Manager`. A connection owns a concrete driver and exposes parameterized execution.

```cpp
auto result = connection->execute(
    "SELECT * FROM users WHERE email = $1",
    {email}
);
```

Bindings are passed separately from the statement. Application code should not interpolate untrusted values into SQL.

A `database::Query` can also carry a statement and its bindings as one value.

## Drivers

Gungnir separates the database contract from vendor adapters. `Driver` is the adapter boundary and `DriverRegistry` registers concrete drivers.

The core recognizes PostgreSQL, MySQL, SQL Server, and MongoDB configuration.

PostgreSQL has an optional libpq adapter built with `GUNGNIR_WITH_POSTGRESQL=ON`; it is exported as `gungnir::postgresql` and registered with `database::register_postgresql()`.

MySQL has an optional native C-client adapter built with `GUNGNIR_WITH_MYSQL=ON`; it is exported as `gungnir::mysql` and registered with `database::register_mysql()`. The build accepts MariaDB Connector/C or a compatible MySQL client library.

SQL Server has an optional ODBC adapter built with `GUNGNIR_WITH_SQLSERVER=ON`; it is exported as `gungnir::sqlserver` and registered with `database::register_sqlserver()`. It targets Microsoft ODBC Driver 18 for SQL Server at runtime.

MongoDB has an optional `libmongoc` adapter built with `GUNGNIR_WITH_MONGODB=ON`; it is exported as `gungnir::mongodb` and registered with `database::register_mongodb()`. MongoDB remains document-native: ORM and migration plans compile to BSON-compatible command documents rather than SQL. Model `id` maps to MongoDB `_id`.

Raw SQL uses the placeholder syntax of the active backend. PostgreSQL uses `$1`, `$2`, and so on; MySQL and SQL Server use `?`. ORM queries compile the correct backend placeholders automatically.

Vendor-specific connection attributes can be supplied through `DB_OPTIONS`. For SQL Server local development with a self-signed certificate, `TrustServerCertificate=yes` can be used; production deployments should validate the server certificate.

## Pooling

`ConnectionPool` creates a bounded set of connections and distributes acquisitions across them. The current pool is deliberately small and deterministic; production queueing and lease semantics belong to later runtime work rather than being hidden behind a misleading API.

## Transactions

`Manager::transaction()` pins work to one connection. `Transaction::run()` commits on success and rolls back on exceptions. Nested transaction/savepoint semantics are explicit driver capabilities and are not emulated by the core.

Connections expose `supports_transactions()` and `supports_savepoints()`. The migration runner honors these capabilities. The initial MongoDB adapter targets standalone deployments and reports transactions as unsupported instead of emulating them; replica-set transaction support is separate work.

## Errors

Driver execution failures are wrapped as `database::Error` with backend, connection name, and statement context. Binding values are intentionally not copied into the exception to reduce accidental credential or personal-data leakage.

## Health

`Connection::healthy()` delegates to the driver ping contract. This is a connectivity signal, not an application readiness guarantee.
