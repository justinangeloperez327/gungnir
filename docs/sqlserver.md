# SQL Server Adapter

> This guide describes the 1.0 contract and its documented limits. See [release scope](release-v1.md).

## Current behavior

The optional adapter requires an ODBC manager plus an installed SQL Server ODBC driver. Build and link it explicitly:

```sh
cmake -S . -B build -DGUNGNIR_WITH_SQLSERVER=ON
cmake --build build --config Release
```

```cmake
target_link_libraries(app PRIVATE gungnir::sqlserver)
```

Register before configuring database connections:

```cpp
gungnir::database::register_sqlserver(app.database_drivers());
app.configure_database();
```

Select `DB_CONNECTION=sqlserver` and configure the server/database/credentials through the common Settings contract. Include `<gungnir/database/sqlserver.hpp>` for registration. Raw query parameter representation: ?.

Live integration tests use `GUNGNIR_SQLSERVER_INTEGRATION_TESTS=ON` and need a reachable correctly configured server. Enabling a build flag does not connect to a database.

## Generated applications

A custom SDK built with this adapter automatically links and registers it for
generated `.gnr` applications. It requires unixODBC and Microsoft ODBC Driver 18
on Linux, or the corresponding installed ODBC driver on Windows. Select the SDK
with `GUNGNIR_CMAKE_PREFIX`, then configure `.env`:

```dotenv
DB_CONNECTION=sqlserver
DB_HOST=127.0.0.1
DB_PORT=1433
DB_DATABASE=your-database
DB_USERNAME=application-user
DB_PASSWORD=your-password
```

Connections use encryption. `DB_OPTIONS=TrustServerCertificate=yes` is used only
for the CI container's test certificate; deployments should validate their
server certificate. Installing an ODBC manager does not install the SQL Server
driver.

Live CI uses SQL Server 2022 and verifies native cancellation, nested savepoint
rollback, after-commit callbacks, migrations, and the installed `.gnr`
application's CRUD, bindings, pagination, relationships and restart persistence.
SQL Server savepoints retain their native semantics; nested commits do not issue
SQL `RELEASE SAVEPOINT`. The standard core/application packages do not bundle
this adapter.

## Limits and planned work

ODBC manager discovery does not install a database driver. Configure encryption and certificate trust explicitly; validate DECIMAL/NUMERIC conversion.

## Implementation references

- [include/gungnir/database/sqlserver.hpp](../include/gungnir/database/sqlserver.hpp)
- [CMakeLists.txt](../CMakeLists.txt)
- [include/gungnir/database/settings.hpp](../include/gungnir/database/settings.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/sqlserver.md).
