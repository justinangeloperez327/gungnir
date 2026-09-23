# MySQL Adapter

Gungnir provides an optional MySQL adapter using the native MySQL C prepared-statement API. The build can link either MariaDB Connector/C or a compatible MySQL client library discovered by Gungnir's CMake module.

Enable it when configuring Gungnir:

```sh
cmake -S . -B build -DGUNGNIR_WITH_MYSQL=ON
```

The exported CMake target is:

```cmake
target_link_libraries(app PRIVATE gungnir::mysql)
```

## Registration

Register the adapter before the application configures its database:

```cpp
gungnir::database::register_mysql(
    app.database_drivers()
);
```

After registration, `DB_CONNECTION=mysql` uses the concrete adapter through the normal Gungnir connection pool, transaction, migration and ORM contracts.

## Parameterization

The adapter uses native prepared statements through `mysql_stmt_prepare`, `mysql_stmt_bind_param` and `mysql_stmt_execute`. Values are never interpolated into SQL.

Raw MySQL statements use `?` placeholders:

```cpp
connection->execute(
    "SELECT * FROM users WHERE email = ?",
    {email}
);
```

ORM queries already compile MySQL placeholders in this form.

## Values

Signed and unsigned integers, booleans and floating-point values are converted into Gungnir model values. Other MySQL values are returned as strings, and SQL NULL is represented as `nullptr`.

MySQL `DECIMAL` and `NEWDECIMAL` currently map to `Double` because the model value type does not yet provide arbitrary-precision decimals.

## Transactions and health

The adapter implements begin, commit, rollback, savepoint capability reporting and `mysql_ping` health checks.

Live integration coverage is enabled with `GUNGNIR_MYSQL_INTEGRATION_TESTS=ON` and requires a reachable MySQL-compatible server.
