# SQL Server Adapter

Gungnir provides an optional SQL Server adapter through ODBC.

Enable it when configuring Gungnir:

```sh
cmake -S . -B build -DGUNGNIR_WITH_SQLSERVER=ON
```

The exported CMake target is:

```cmake
target_link_libraries(app PRIVATE gungnir::sqlserver)
```

## Runtime dependency

The adapter links against the platform ODBC manager. A SQL Server ODBC driver must also be installed at runtime. Gungnir currently targets **Microsoft ODBC Driver 18 for SQL Server**.

## Registration

Register the adapter before the application configures its database:

```cpp
gungnir::database::register_sqlserver(
    app.database_drivers()
);
```

Use `DB_CONNECTION=mssql`, `sqlserver`, or `sql_server`.

## Parameterization

The adapter uses ODBC prepared statements and `SQLBindParameter`. Bound values are passed separately from SQL and are never interpolated into the statement.

Raw SQL Server statements use `?` placeholders:

```cpp
connection->execute(
    "SELECT * FROM users WHERE email = ?",
    {email}
);
```

## Encryption and connection options

ODBC Driver 18 uses encrypted connections. Additional ODBC connection attributes can be supplied through `DB_OPTIONS`. For local development with a self-signed SQL Server certificate, `TrustServerCertificate=yes` can be used. Production deployments should validate the server certificate rather than disabling certificate verification.

## Values

BIT, signed integer, floating-point and numeric values map to Gungnir scalar values. Other values are returned as strings, and SQL NULL maps to `nullptr`.

SQL Server DECIMAL/NUMERIC values currently map to `Double` because Gungnir does not yet expose an arbitrary-precision decimal scalar.

## Transactions and health

The adapter uses ODBC autocommit control plus `SQLEndTran` for commit and rollback, reports savepoint capability, and uses a lightweight `SELECT 1` health check.

Live integration coverage is enabled with `GUNGNIR_SQLSERVER_INTEGRATION_TESTS=ON`.
