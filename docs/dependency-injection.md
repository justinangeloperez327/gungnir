# Dependency Injection

## Overview
Native Application provides `bind`, `singleton`, `scoped`, `instance` and `resolve`. The Container owns bindings and request service scopes. Controllers can use `inject Type name;` to request generated constructor/member plumbing.

Register dependencies before resolving the controller. Request-scoped dependencies should be resolved through request services.

## Scope
General automatic constructor discovery and every target declaration's injection rules are not established by controller injection support. Keep native registrations explicit, and do not retain request-scoped services past their scope lifetime.



- [include/gungnir/core/application.hpp](../include/gungnir/core/application.hpp)
- [include/gungnir/core/container.hpp](../include/gungnir/core/container.hpp)
- [src/language/controller_lowering.cpp](../src/language/controller_lowering.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/dependency-injection.md).
