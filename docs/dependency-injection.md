# Dependency Injection

## Overview
Native Application provides `bind`, `singleton`, `scoped`, `instance` and `resolve`. The Container owns bindings and request service scopes. Controllers can use `inject Type name;` to request generated constructor/member plumbing.

Register dependencies before resolving the controller. Request-scoped dependencies should be resolved through request services.

## Scope
Register dependency factories and adapters explicitly. Generated controllers retain the owners of injected dependencies through awaited calls. Resolve request-scoped dependencies through the request service container and release them with their request scope. Application services such as `Cache` and `Storage` are configured through `ServicesProvider`.



- [include/gungnir/core/application.hpp](../include/gungnir/core/application.hpp)
- [include/gungnir/core/container.hpp](../include/gungnir/core/container.hpp)
- [Canonical validation](../src/language/validated.cpp)
- [Structural C++ IR](../src/language/cpp_ir.cpp)

See the [documentation index](README.md), [getting started](getting-started.md), and [target design](design/dependency-injection.md).
