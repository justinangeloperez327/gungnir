# Models

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/model.md).

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

## Limits and planned work

The metadata-only model design is a planned replacement for this field-based lowering. Its `primaryKey`, `fillable`, `hidden`, `casts`, string-named relationships, polymorphic relationships and relationship modifiers must not be assumed to have matching `.gnr` support today. The native model APIs and accepted model declaration grammar are different surfaces.

## Implementation references

- [src/language/model_lowering.cpp](../src/language/model_lowering.cpp)
- [include/gungnir/model/model.hpp](../include/gungnir/model/model.hpp)
- [include/gungnir/model/metadata.hpp](../include/gungnir/model/metadata.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/model.md).
