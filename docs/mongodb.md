# MongoDB Adapter

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/mongodb.md).

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

## Limits and planned work

MongoDB is a document backend. SQL joins, DDL, foreign keys and migration semantics are not portable to it; transaction support depends on server topology.

## Implementation references

- [include/gungnir/database/mongodb.hpp](../include/gungnir/database/mongodb.hpp)
- [CMakeLists.txt](../CMakeLists.txt)
- [include/gungnir/database/settings.hpp](../include/gungnir/database/settings.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/mongodb.md).
