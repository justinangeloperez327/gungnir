# Collections

ORM queries return `Collection<Model>`, an owned sequence of hydrated models.
Iterate the result or use callbacks to transform it in memory:

```gnr
const users = User::all();
const active = users.filter((user) => user.active).sortBy((user) => user.name);
const names = active.map((user) => user.name);
const total = active.reduce(0, (sum, user) => sum + user.id);
```

`filter` and `reject` retain `Collection<Model>`. `map` returns `List<Result>`.
`each` invokes its callback and returns `void`.

## Language operations

| Operation | Result |
| --- | --- |
| `empty`, `isEmpty`, `contains`, `every` | `bool` |
| `size`, `count` | `int` |
| `first`, `last`, `at(index)` | A model; empty or out-of-range access throws |
| `find(key)` | `Model?`, searched by the model's primary key |
| `filter`, `reject` | A collection selected by a boolean callback |
| `sortBy(projection, descending)`, `unique(projection)` | A collection ordered or deduplicated by a scalar projection |
| `take(count)`, `skip(count)` | A collection slice |
| `chunk(count)` | `List<Collection<Model>>`; chunk size must be positive |
| `sum(projection)` | The projected numeric type; integral overflow throws |
| `reduce(initial, callback)` | The initial value's type; the callback receives accumulator and model |
| `values()` | `List<Model>` |

Indexes and slice sizes must be nonnegative. `sortBy` defaults to ascending order.
Collection callbacks and traversal perform no additional database queries.

## Native C++ operations

`gungnir::orm::Collection<Model>` owns a `std::vector<Model>` and also exposes
the following native APIs:

| Operation | Behavior |
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

See [ORM](orm.md) for database queries and
[the native collection API](../include/gungnir/orm/collection.hpp) for C++ signatures.
