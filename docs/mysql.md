# MySQL Adapter

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

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

## Limits and planned work

DECIMAL/NEWDECIMAL currently map to Double; use explicit conversion where exact decimal semantics matter.

## Implementation references

- [include/gungnir/database/mysql.hpp](../include/gungnir/database/mysql.hpp)
- [CMakeLists.txt](../CMakeLists.txt)
- [include/gungnir/database/settings.hpp](../include/gungnir/database/settings.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/mysql.md).
