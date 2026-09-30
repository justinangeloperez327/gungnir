# Policies and Authorization

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/policy.md).

## Current behavior

Structured policy methods infer `auth::Decision`, including conversion from bool returns. Generated `register_policy` binds public synchronous actor/resource methods to `auth::ResourceAuthorization`. The registry validates the actor/resource types and denies unknown or mismatched abilities.

## Limits and planned work

The existing Identity-only `Authorization` API remains separate. Async resource-policy registration and automatic ORM/authenticated-user resolution are not implemented.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/auth/authorization.hpp](../include/gungnir/auth/authorization.hpp)
- [include/gungnir/auth/authorize.hpp](../include/gungnir/auth/authorize.hpp)
- [include/gungnir/core/framework_artifacts.hpp](../include/gungnir/core/framework_artifacts.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/policy.md).
