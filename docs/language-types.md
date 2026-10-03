# Language Types

> **Status: Development.** This guide describes the current implementation and documented limits. The 1.0 compatibility contract is not frozen yet.

## Current behavior

The structured validator interns resolved types and records a TypeId for every expression. Known calls, return paths, const writes, conditions, callback signatures, optionals and declared field access are checked. Integer literals are range-checked, decimal/hex/binary literals and separators are normalized, and single/double quotes both create strings. Unicode escapes must denote valid scalar values.

Common-type selection is centralized for conditional expressions, inferred returns and arithmetic results. It is symmetric: operand/branch order does not change the semantic result. `null` combined with a concrete value infers an optional `T?`; matching decimal arithmetic retains the semantic `decimal` type; and mixed numeric families are accepted only through the current widening table (`int -> double`, `int -> decimal`, `uint64 -> decimal`). Signed/unsigned mixing, `uint64 -> double`, and implicit `double <-> decimal` conversion are rejected.

## Limits and planned work

The existing numeric `decimal` alias uses double precision. `Decimal` is a separate exact stored value; `exactDecimal('123.4500')` constructs it from a string, and model `decimal` casts use it. `string()` preserves its representation; `toDouble()` explicitly converts to binary floating point. It has no implicit arithmetic conversion. Unknown native types/calls require explicit compiler bindings. Collection type inference may use JSON values for heterogeneous lists; runtime model casting is a separate concern.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/language/types.hpp](../include/gungnir/language/types.hpp)
- [include/gungnir/language/type_system.hpp](../include/gungnir/language/type_system.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/language-types.md).
