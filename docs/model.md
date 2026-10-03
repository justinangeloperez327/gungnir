# Models

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

## Current behavior

The authoritative structured compiler supports model metadata directly in the canonical syntax/validated-AST/C++ IR pipeline. Current metadata includes:

- `table` and `connection`;
- `primaryKey` and `incrementing`;
- `fillable`, `hidden`, and `visible`;
- `casts`;
- `timestamps`;
- `softDeletes`.

Metadata is semantically validated before C++ generation. Duplicate or unknown metadata is rejected. Attribute lists require constant strings, casts require supported constant cast names, boolean metadata requires boolean constants, and primary-key/incrementing combinations are checked before code generation.

Metadata-only models synthesize the attributes needed by their declared persistence contract. `fillable`, `hidden`, `visible`, and `casts` contribute attributes; the primary key is always present; timestamps synthesize nullable `created_at` and `updated_at`; soft deletes synthesize nullable `deleted_at`. Generated model fields and native metadata therefore come from the validated model contract rather than a later raw-source scan.

Use migrations for database schema changes. Model metadata describes persistence and serialization behavior; it does not create database columns.

## Example

```gnr
model User {
    table = 'users';
    primaryKey = 'id';

    fillable = [
        'name',
        'email'
    ];

    hidden = [
        'password'
    ];

    casts = {
        'active': 'bool',
        'settings': 'json'
    };

    timestamps = true;
    softDeletes = true;
}
```

The generated model inherits the ORM behavior. JSON/view serialization honors generated `hidden` and `visible` metadata, with hidden fields taking precedence. Persistence and dirty tracking continue to use the full generated attribute metadata.

Hydrated models synchronize original values and begin clean. Runtime conversion rejects incompatible field values. Successful persistence synchronizes dirty state; a zero-row update does not falsely clean the model. Eager loading batches parent keys and releases ordinary query leases before related queries are loaded, allowing deterministic behavior even with a one-connection pool.

## Relationships

The native ORM already implements HasOne, HasMany, BelongsTo, BelongsToMany, HasOneThrough, and HasManyThrough, including generated relation metadata and eager-loading support.

The older compatibility parser/model lowerer also recognizes typed relationship declarations such as `hasMany<Post>()`. That compatibility support is not the canonical structured compiler contract. Group 1 must not treat it as proof that the canonical `SyntaxParser -> ProgramValidator -> C++ IR` relationship surface is complete.

See [ORM Relationships](relationships.md) for the current relationship boundary.

## Limits and planned work

The remaining Group 1 work is primarily the canonical structured relationship/query surface, not the basic model metadata contract.

In particular, do not yet assume that the design-only string-resource relationship syntax, polymorphic relationships, one-of-many modifiers, constrained eager-loading callbacks, or every documented ORM callback/query form is implemented in the authoritative structured compiler.

The legacy field-based model lowering path remains compatibility code. New Group 1 work should converge on the authoritative structured compiler rather than adding new framework behavior only to the compatibility parser.

See [Database and ORM Correctness](database-correctness.md) for persistence, hydration, eager-loading, transaction, and pool invariants.

## Implementation references

- [src/language/syntax.cpp](../src/language/syntax.cpp)
- [src/language/validated.cpp](../src/language/validated.cpp)
- [src/language/cpp_ir.cpp](../src/language/cpp_ir.cpp)
- [src/language/model_lowering.cpp](../src/language/model_lowering.cpp)
- [include/gungnir/model/model.hpp](../include/gungnir/model/model.hpp)
- [include/gungnir/model/metadata.hpp](../include/gungnir/model/metadata.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/model.md).
