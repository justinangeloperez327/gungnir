# Dependency Injection

> **Future design specification.** This document describes intended long-term architecture beyond the published `1.0` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../dependency-injection.md) before using an API.


## Release alignment

This document describes possible future architecture beyond 1.0. Current implementation guides and executable tests define the published contract.

Gungnir dependency injection separates application dependencies from construction mechanics.

Application-facing declarations use inject:

~~~gnr
controller UserController {
    inject UserService users;

    public show(int id) {
        return json(users.find(id));
    }
}
~~~

The compiler resolves UserService as a type and the runtime container supplies the instance.

# Responsibilities

Dependency injection provides:

- service registration;
- constructor/factory resolution;
- lifetime management;
- scoped request services;
- test overrides;
- circular dependency detection.

It does not replace modules/imports.

Import makes a type visible to the compiler.

inject requests an instance from the application container.

# Lifetimes

Supported container lifetimes are:

~~~text
transient
singleton
scoped
instance
~~~

Transient:
a new instance is created for each resolution.

Singleton:
one application-owned instance is reused.

Scoped:
one instance is reused only inside an explicit request/operation scope.

Instance:
an already-created instance is registered.

# Scoped services

Scoped bindings must only resolve inside an active scope.

Resolving a scoped service without a scope is an error.

HTTP integration should create and destroy request scopes automatically.

Controllers should not call begin_scope/end_scope manually.

# Framework injection

Framework declarations that support injection include:

- controllers;
- middleware;
- policies;
- listeners;
- notifications/mail where allowed by their contracts.

Each declaration's canonical documentation defines whether inject is legal.

# Type-based resolution

The compiler resolves injected type syntax to TypeId/SymbolId before lowering.

Generated code should use resolved container metadata rather than repeated runtime string lookup where possible.

# Factories

Bindings may use factories when construction requires configuration or runtime state.

Factory cycles must still be detected.

A factory must not silently change the declared service lifetime.

# Interfaces and abstractions

Where an interface/contract abstraction is available, the container may bind the contract to an implementation.

Until a first-class Gungnir interface syntax exists, native/runtime services may expose explicit binding metadata.

Application code should not depend on C++ pointer ownership syntax.

# Automatic construction

Unregistered concrete native services may be auto-constructible only when the runtime can do so safely and predictably.

Automatic construction must not hide ambiguous constructors or lifetime requirements.

Explicit registration is preferred for infrastructure services.

# Circular dependencies

Circular resolution must fail with a readable dependency path.

Example:

~~~text
A -> B -> C -> A
~~~

Do not recurse until stack failure.

# Overrides

Bindings may be replaced deliberately for:

- tests;
- application customization;
- package overrides.

Test overrides should be scoped to the test/application container and not mutate unrelated process-global state.

# Async/request scope safety

A scoped service used across await must remain attached to the logical request/operation scope.

Scope ownership must follow coroutine context rather than thread identity.

# Shutdown

Singleton/application-owned services that require shutdown should participate in the application lifecycle through providers/runtime hooks.

The container should not depend on unspecified C++ global destruction order.

# Compiler contract

Source:

~~~gnr
inject UserService users;
~~~

should become validated metadata similar to:

~~~text
ValidatedInjection
  symbol = users
  requestedType = UserService
  containerResolution = resolved
~~~

Lowering generates constructor/container plumbing.

The transpiler must not rediscover the injected type from source text.

# Design rule

~~~text
imports resolve names
inject resolves instances
container owns lifetime
application lifecycle owns shutdown
~~~

