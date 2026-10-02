# Models

> **Status: experimental, pre-1.0.** Phase 12 hardens the runtime ORM behavioral baseline. Compiler-level model semantics that remain partial are tracked separately.

## Current behavior

Current model lowering accepts model fields and generates native model members, attribute metadata and fillable entries from them. It recognizes table, connection, timestamps and soft-delete configuration. The generated model inherits the ORM behavior.

Use migrations for actual database schema changes; a generated model does not create its database table.

## Example

```gnr
model User {
    string name;
    string email;
}
```

The structured compiler emits native attribute descriptors and table/connection/fillable/hidden/visible/cast metadata. Metadata-only attributes receive inferred fields. JSON and view serialization honor native/generated `hidden` and `visible` metadata; hidden fields always win. Persistence and dirty tracking still use all attributes. Decimal casts generate exact `model::Decimal` fields. Other runtime casting follows each declared field type.

Hydrated models synchronize original values and begin clean. Runtime conversion rejects integer overflow, negative-to-unsigned assignment and incompatible field types. Successful persistence synchronizes dirty state; a zero-row update returns false without falsely cleaning the model. Eager loading batches parent keys and releases ordinary query leases before related queries are loaded, allowing deterministic behavior even with a one-connection pool.

## Limits and planned work

The metadata-only model design is a planned replacement for this field-based lowering. Its `primaryKey`, `fillable`, `hidden`, `casts`, string-named relationships, polymorphic relationships and relationship modifiers must not be assumed to have matching `.gnr` support today. The native model APIs and accepted model declaration grammar are different surfaces.

See [Database and ORM Correctness](database-correctness.md) for persistence, hydration, eager-loading, transaction and pool invariants.

## Implementation references

- [src/language/model_lowering.cpp](../src/language/model_lowering.cpp)
- [include/gungnir/model/model.hpp](../include/gungnir/model/model.hpp)
- [include/gungnir/model/metadata.hpp](../include/gungnir/model/metadata.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/model.md).
