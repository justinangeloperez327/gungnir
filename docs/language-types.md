# Language Types

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/language-types.md).

## Current behavior

TypeSystem models unknown, null, boolean, integer, decimal, string, optional, list, map and named types. Known numeric literals and selected declarations/expressions receive type checks.

## Limits and planned work

Types are not yet a fully resolved TypeId graph. Unknown native types remain permissive. The decimal type category is not an exact fixed/arbitrary-precision runtime guarantee; optional/generic syntax requires validation against the accepted frontend subset.

## Implementation references

- [include/gungnir/language/types.hpp](../include/gungnir/language/types.hpp)
- [include/gungnir/language/type_system.hpp](../include/gungnir/language/type_system.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/language-types.md).
