# Collections

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/collection.md).

## Current behavior

`gungnir::orm::Collection<Model>` owns a `std::vector<Model>`. ORM queries use it for materialized model results.

| Operation | Current behavior |
| --- | --- |
| `empty`, `size`, `count` | Inspect the sequence |
| `first`, `last` | Return a model reference; throw `std::out_of_range` when empty |
| `at` | Bounds-checked access |
| `operator[]` | Unchecked indexed access |
| `find` | Search primary key; return `std::optional<Model>` |
| `pluck` | Return a vector of attribute values |
| `filter` | Return another model collection |
| `each` | Invoke a callback; return void |
| `map` | Return `std::vector<Result>`, not `Collection<Result>` |
| `push`, `clear`, `values` | Mutate or access backing storage |
| `begin`, `end` | Iterate models |

## Limits and planned work

Predicate overloads of `first`/`last`, optional empty results, `groupBy`, `reduce`, sorting, aggregation, flattening, lazy collections and the wider Laravel-style catalogue are planned APIs. A `.gnr` arrow callback is not established merely because a native callback overload exists.

## Implementation references

- [include/gungnir/orm/collection.hpp](../include/gungnir/orm/collection.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/collection.md).
