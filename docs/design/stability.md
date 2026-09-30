# Stability

> **Design specification.** This document preserves the intended contract. Examples and requirements below may exceed the current implementation. See the [current implementation guide](../stability.md) before using an API.

Gungnir is pre-1.0.

The framework, source language, compiler internals, and runtime APIs are still allowed to evolve, but breaking changes should be intentional rather than accidental.

# Stability layers

Gungnir has several different compatibility surfaces:

~~~text
Gungnir source language
application-facing framework API
compiler CLI
generated C++ ABI
native runtime C++ API
package/plugin API
database/storage adapter contracts
~~~

These layers do not need identical stability guarantees.

# Source language

Canonical language behavior is defined by:

~~~text
grammar.md
language-types.md
expressions.md
statements.md
functions.md
async.md
modules.md
ast.md
semantics.md
validated-ast.md
transpiler.md
~~~

Before 1.0, syntax may change when necessary to make the language coherent.

Once a syntax is declared stable, changes should use migration/deprecation guidance where practical.

# Application-facing framework API

Canonical framework behavior is defined by the concept-specific docs such as:

~~~text
model.md
orm.md
migration.md
controller.md
routing.md
middleware.md
request.md
response.md
validation.md
collection.md
authentication.md
policy.md
event.md
listener.md
notification.md
mail.md
view.md
~~~

Documentation must not advertise aspirational behavior as implemented.

# Native runtime API

Headers under include/gungnir are a candidate native integration surface.

Before 1.0, compatibility is not guaranteed.

Native users should expect changes as the source language and runtime boundaries mature.

# Generated C++

Generated C++ is not a stable public API.

Projects should rebuild generated code with the matching compiler/runtime version.

Applications should not manually depend on generated namespaces, helper names, or native class layout.

# Runtime/compiler compatibility

The compiler and runtime should carry an internal compatibility/ABI version.

Generated code must not silently compile against an incompatible runtime contract.

# Deprecation

After a stable 1.x release, public application-facing removals should normally pass through a deprecation period unless:

- a security issue requires immediate removal;
- the old behavior cannot be made safe;
- compatibility would preserve incorrect semantics.

# Capability claims

A type, stub, interface, or experimental branch is not proof that a feature is production-ready.

Docs should distinguish:

~~~text
implemented
experimental
partial
planned
unsupported
~~~

# Concurrency claims

An API is not thread-safe, coroutine-safe, or distributed-safe merely because it compiles in concurrent code.

Guarantees must be documented per subsystem and backed by implementation/tests.

# Backend portability

A Gungnir API may be portable while a specific backend lacks a capability.

Backend docs must state such limitations clearly.

The compiler/runtime should report unsupported selected-backend operations before production execution where practical.

# Plugins/packages

Package compatibility metadata is meaningful only when a version-range grammar and enforcement mechanism exist.

Until then, compatibility declarations are descriptive.

# Documentation authority

When old notes conflict with the canonical language/framework docs, the canonical docs win.

Historical group/branch notes should not remain in the main documentation set.

# Design rule

~~~text
pre-1.0 allows evolution
but every breaking change should improve a defined contract
and documentation must match the repository's real capabilities
~~~

