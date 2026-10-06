# MySQL Adapter

> This guide describes the 1.0 contract and its documented limits. See [release scope](release-v1.md).

## Current behavior

The optional adapter requires MariaDB Connector/C or a compatible MySQL C client. Build and link it explicitly:

```sh
cmake -S . -B build -DGUNGNIR_WITH_MYSQL=ON
cmake --build build --config Release
```

```cmake
target_link_libraries(app PRIVATE gungnir::mysql)
```

Register before configuring database connections:

```cpp
gungnir::database::register_mysql(app.database_drivers());
app.configure_database();
```

Select `DB_CONNECTION=mysql` and configure the server/database/credentials through the common Settings contract. Include `<gungnir/database/mysql.hpp>` for registration. Raw query parameter representation: ?.

Live integration tests use `GUNGNIR_MYSQL_INTEGRATION_TESTS=ON` and need a reachable correctly configured server. Enabling a build flag does not connect to a database.

## Generated applications

A custom SDK built with this adapter automatically links and registers it for
generated `.gnr` applications. Select that SDK with `GUNGNIR_CMAKE_PREFIX` and
configure the application's `.env`:

```dotenv
DB_CONNECTION=mysql
DB_HOST=127.0.0.1
DB_PORT=3306
DB_DATABASE=gungnir
DB_USERNAME=application-user
DB_PASSWORD=your-password
```

Live CI uses MySQL 8.4 and verifies installed application migrations, model
persistence, bound queries, pagination, relationships and nested transactions.
Foreign IDs must use a type compatible with the referenced primary key; use
`table.foreignId(...)` for a generated `table.id()` key. Both generate unsigned
`BIGINT` on MySQL. Existing tables require matching key types when adding foreign
keys. Savepoint commands use the direct protocol; query values remain prepared
bindings. The standard
core/application packages do not bundle this adapter.

## Limits and planned work

DECIMAL/NEWDECIMAL currently map to Double; use explicit conversion where exact decimal semantics matter.

## Implementation references

- [include/gungnir/database/mysql.hpp](../include/gungnir/database/mysql.hpp)
- [CMakeLists.txt](../CMakeLists.txt)
- [include/gungnir/database/settings.hpp](../include/gungnir/database/settings.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/mysql.md).
