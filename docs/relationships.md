# ORM Relationships

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

## Current behavior

Native relationship wrappers cover HasOne, HasMany, BelongsTo, BelongsToMany, HasOneThrough and HasManyThrough. Generated metadata drives eager loading and relationship access.

Eager loading batches parent keys. Loaded state distinguishes an unloaded relation from a loaded empty relation; accessing an unloaded relation raises `RelationNotLoaded`. Pivot mutation APIs operate through the relationship runtime.

## Limits and planned work

The planned `posts() { return hasMany("posts"); }` syntax, polymorphic relations and one-of-many modifiers have a separate implementation path. Do not infer those capabilities from the six existing native wrapper types. Use explicit eager loading and validate backend support.

## Implementation references

- [include/gungnir/model/relation.hpp](../include/gungnir/model/relation.hpp)
- [include/gungnir/orm/relation_query.hpp](../include/gungnir/orm/relation_query.hpp)
- [include/gungnir/orm/relation_mutation.hpp](../include/gungnir/orm/relation_mutation.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/relationships.md).
