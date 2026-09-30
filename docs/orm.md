# ORM

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/orm.md).

## Current behavior

The native ORM supplies model queries, hydration, persistence, pagination, soft-delete operations and relationship loading/mutations. Multi-model results use `orm::Collection<Model>`; pagination returns `orm::Page<Model>`.

The lowerer maps selected camelCase call names to native names. Use typed controller actions and the currently supported model shape. A terminal query performs database work; collections operate on already-loaded values.

## Limits and planned work

The target catalogue includes query methods, callbacks, lifecycle hooks and model metadata beyond current lowering. Inspect the Query/Model signature and lowerer alias table before adopting an example. Target string-named relationship declarations and arrow query closures are not automatically implemented. SQL operations do not all map to MongoDB.

## Implementation references

- [include/gungnir/model/model.hpp](../include/gungnir/model/model.hpp)
- [include/gungnir/orm/query.hpp](../include/gungnir/orm/query.hpp)
- [include/gungnir/orm/advanced.hpp](../include/gungnir/orm/advanced.hpp)
- [src/language/model_lowering.cpp](../src/language/model_lowering.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/orm.md).
