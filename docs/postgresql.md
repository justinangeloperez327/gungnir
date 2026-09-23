# PostgreSQL Adapter

Gungnir provides an optional PostgreSQL adapter backed by libpq.

The adapter is separate from the core library. Enable it when configuring Gungnir:

```sh
cmake -S . -B build -DGUNGNIR_WITH_POSTGRESQL=ON
```

The exported CMake target is:

```cmake
target_link_libraries(app PRIVATE gungnir::postgresql)
```

## Registration

Register the adapter before the application configures its database:

```cpp
gungnir::database::register_postgresql(
    app.database_drivers()
);
```

After registration, the existing `DB_CONNECTION=postgresql` configuration, connection pool, migrations, transactions, ORM execution and health checks use the libpq adapter.

## Parameterization

The adapter uses `PQexecParams`. Binding values are passed separately from SQL and are never interpolated into the statement.

Raw PostgreSQL statements use PostgreSQL placeholders:

```cpp
connection->execute(
    "SELECT * FROM users WHERE email = $1",
    {email}
);
```

ORM queries already compile PostgreSQL placeholders in this form.

## Value mapping

PostgreSQL booleans, signed integers, floating-point values and numeric values are mapped into Gungnir model values. Other PostgreSQL types are returned as strings. SQL NULL is returned as `nullptr`.

The current model value type does not provide an arbitrary-precision decimal type, so PostgreSQL `NUMERIC` values are represented as `Double`. Applications requiring exact arbitrary-precision decimal arithmetic should not rely on that conversion.

## Transactions and health

The adapter implements begin, commit, rollback, savepoint capability reporting and a live `SELECT 1` health check.

## Integration testing

Live integration coverage is opt-in:

```sh
cmake -S . -B build \
  -DGUNGNIR_WITH_POSTGRESQL=ON \
  -DGUNGNIR_POSTGRESQL_INTEGRATION_TESTS=ON
```

The integration test expects a PostgreSQL server and reads the `GUNGNIR_POSTGRESQL_HOST`, `GUNGNIR_POSTGRESQL_PORT`, `GUNGNIR_POSTGRESQL_DATABASE`, `GUNGNIR_POSTGRESQL_USERNAME` and `GUNGNIR_POSTGRESQL_PASSWORD` environment variables.
