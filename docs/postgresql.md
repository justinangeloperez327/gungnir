# PostgreSQL Adapter

> This guide describes the 1.0 contract and its documented limits. See [release scope](release-v1.md).

## Current behavior

The optional adapter requires libpq. Build and link it explicitly:

```sh
cmake -S . -B build -DGUNGNIR_WITH_POSTGRESQL=ON
cmake --build build --config Release
```

```cmake
target_link_libraries(app PRIVATE gungnir::postgresql)
```

Register before configuring database connections:

```cpp
gungnir::database::register_postgresql(app.database_drivers());
app.configure_database();
```

Select `DB_CONNECTION=postgresql` and configure the server/database/credentials through the common Settings contract. Include `<gungnir/database/postgresql.hpp>` for registration. Raw query parameter representation: $1, $2, ....

Live integration tests use `GUNGNIR_POSTGRESQL_INTEGRATION_TESTS=ON` and need a reachable correctly configured server. Enabling a build flag does not connect to a database.

## Generated applications

A custom SDK built with this adapter automatically links and registers it for
generated `.gnr` applications. Select that SDK with `GUNGNIR_CMAKE_PREFIX` and
configure the application's `.env`:

```dotenv
DB_CONNECTION=postgresql
DB_HOST=127.0.0.1
DB_PORT=5432
DB_DATABASE=gungnir
DB_USERNAME=postgres
DB_PASSWORD=your-password
```

Live CI uses PostgreSQL 16 and verifies installed application migrations, model
persistence, bound queries, pagination, relationships and nested transactions.
The standard core/application packages do not bundle this adapter.

## Limits and planned work

Check NUMERIC/DECIMAL conversions and backend capability reporting before depending on exact decimal semantics.

## Implementation references

- [include/gungnir/database/postgresql.hpp](../include/gungnir/database/postgresql.hpp)
- [CMakeLists.txt](../CMakeLists.txt)
- [include/gungnir/database/settings.hpp](../include/gungnir/database/settings.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/postgresql.md).
