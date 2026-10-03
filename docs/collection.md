# Collections

> **Status: Gungnir 1.0 current contract.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/collection.md).

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
| `contains`, `every`, `reject` | Evaluate predicates or exclude matching items |
| `take`, `skip`, `chunk` | Slice or group items; zero chunk size is rejected |
| `reduce`, `sum` | Fold values; integral sums reject overflow |
| `sort_by`, `unique` | Stable projection sorting or first-occurrence deduplication |
| `group_by`, `key_by` | Build maps from projected keys |

## Limits and planned work

Predicate overloads of `first`/`last`, optional empty results, additional aggregation, flattening, lazy collections and the wider Laravel-style catalogue are planned APIs. A `.gnr` arrow callback is not established merely because a native callback overload exists.

## Implementation references

- [include/gungnir/orm/collection.hpp](../include/gungnir/orm/collection.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/collection.md).
