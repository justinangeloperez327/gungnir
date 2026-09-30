# Migrations

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/migration.md).

The structured profile (`gungnirc --strict`) supports typed functions and framework actions, structured callbacks, and validated C++ emission. See [compiler profiles](compiler-profiles.md) for usage and current limits. The compatibility profile retains the native syntax described below.

## Current behavior

The migration runtime builds table operations and executes them through a migration runner and selected backend. Native `Table` supports `create`, `alter`, `rename`, `drop`, and `drop_if_exists`. Column builders include scalar types, indexes, foreign keys, timestamps and soft-delete columns.

Use `gungnir make:migration <name>` to inspect the current generated declaration. Apply migrations with `gungnir migrate`; use `migrate:plan` and `migrate:status` to inspect them.

## Example

Native operation fragment (inside migration planning):

```cpp
gungnir::migration::Table::create("users", [](gungnir::migration::Column& table) {
    table.id();
    table.string("name");
    table.timestamps();
});
```

## Limits and planned work

The native callback receives `Column&`. The structured profile supports `(table) => { ... }` callbacks and validated schema calls. Compatibility mode requires native callbacks. Backend DDL and transaction capabilities differ.

## Implementation references

- [include/gungnir/migration/table.hpp](../include/gungnir/migration/table.hpp)
- [include/gungnir/migration/column.hpp](../include/gungnir/migration/column.hpp)
- [include/gungnir/migration/runner.hpp](../include/gungnir/migration/runner.hpp)
- [src/language/migration_lowering.cpp](../src/language/migration_lowering.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/migration.md).
