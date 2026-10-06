# MongoDB Adapter

> This guide describes the 1.0 contract and its documented limits. See [release scope](release-v1.md).

## Current behavior

The optional adapter requires libmongoc/libbson. Build and link it explicitly:

```sh
cmake -S . -B build -DGUNGNIR_WITH_MONGODB=ON
cmake --build build --config Release
```

```cmake
target_link_libraries(app PRIVATE gungnir::mongodb)
```

Register before configuring database connections:

```cpp
gungnir::database::register_mongodb(app.database_drivers());
app.configure_database();
```

Select `DB_CONNECTION=mongodb` and configure the server/database/credentials through the common Settings contract. Include `<gungnir/database/mongodb.hpp>` for registration. Raw query parameter representation: backend-specific document query representation.

Live integration tests use `GUNGNIR_MONGODB_INTEGRATION_TESTS=ON` and need a reachable correctly configured server. Enabling a build flag does not connect to a database.

## Generated applications

A custom SDK built with this adapter automatically links and registers it for
generated `.gnr` applications. The build requires MongoDB C driver 2.5.4 or newer.
Select the SDK with `GUNGNIR_CMAKE_PREFIX`, then configure `.env`:

```dotenv
DB_CONNECTION=mongodb
DB_HOST=127.0.0.1
DB_PORT=27017
DB_DATABASE=your-database
DB_USERNAME=application-user
DB_PASSWORD=your-password
```

Optional URI settings can be supplied through `DB_OPTIONS`. Live CI uses MongoDB
8.0 and a pinned C driver 2.5.4 source build. It verifies native collection
migrations and an installed `.gnr` application's model persistence, bound
queries, nullable/hidden values, pagination, eager/inverse relationships,
restart persistence and migration rollback.

Models use integer sequence keys by default. Relationships and `with()` use
document queries; SQL `join()` and row-lock clauses are rejected. Collection
migrations apply validators and indexes, but declared foreign keys are not
enforced by MongoDB. Migration records and multi-command schema changes are not
atomic.

## Limits and planned work

The current adapter does not implement transaction sessions or savepoints on
any server topology. `database.transaction(...)` rejects work before the callback
runs. A replica set alone does not enable Gungnir transactions. The standard
core/application packages do not bundle this adapter.

## Implementation references

- [include/gungnir/database/mongodb.hpp](../include/gungnir/database/mongodb.hpp)
- [CMakeLists.txt](../CMakeLists.txt)
- [include/gungnir/database/settings.hpp](../include/gungnir/database/settings.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/mongodb.md).
