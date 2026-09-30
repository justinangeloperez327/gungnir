# Dependency Injection

> **Status: experimental, pre-1.0.** This page describes the current implementation. Native C++ APIs and `.gnr` syntax are identified separately. Proposed contracts are in the [design specification](design/dependency-injection.md).

## Current behavior

Native Application provides `bind`, `singleton`, `scoped`, `instance` and `resolve`. The Container owns bindings and request service scopes. Controllers can use `inject Type name;` to request generated constructor/member plumbing.

Register dependencies before resolving the controller. Request-scoped dependencies should be resolved through request services.

## Limits and planned work

General automatic constructor discovery and every target declaration's injection rules are not established by controller injection support. Keep native registrations explicit, and do not retain request-scoped services past their scope lifetime.

## Implementation references

- [include/gungnir/core/application.hpp](../include/gungnir/core/application.hpp)
- [include/gungnir/core/container.hpp](../include/gungnir/core/container.hpp)
- [src/language/controller_lowering.cpp](../src/language/controller_lowering.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/dependency-injection.md).
