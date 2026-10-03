# Policies and Authorization

> **Status: Gungnir 1.0 current contract.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/policy.md).

## Current behavior

Structured policy methods infer `auth::Decision`, including conversion from bool expressions. Every public policy ability is now required to be synchronous, return the logical `Decision` type, and accept exactly two non-optional model parameters: actor and resource. Generated `register_policy` binds those methods to `auth::ResourceAuthorization`. Invalid policy surfaces stop at semantic validation with `GNR2304` instead of being silently skipped by registration. Register an authenticated actor resolver with `ResourceAuthorization::actor<Actor>` and register generated policies during application bootstrap. Structured `.gnr` handlers can call `authorize(request, 'view', resource)` using the request service scope. Guests receive 401; denied, missing or ambiguous policy bindings receive 403.

## Limits and planned work

The existing Identity-only `Authorization` API remains separate. Async resource-policy registration and automatic ORM/authenticated-user resolution are not implemented.

## Implementation references

- [Structured compiler API](../include/gungnir/language/compiler.hpp)
- [Structured compiler tests](../tests/structured_language.cpp)

- [include/gungnir/auth/authorization.hpp](../include/gungnir/auth/authorization.hpp)
- [include/gungnir/auth/authorize.hpp](../include/gungnir/auth/authorize.hpp)
- [include/gungnir/core/framework_artifacts.hpp](../include/gungnir/core/framework_artifacts.hpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/policy.md).
