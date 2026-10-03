# Packages and Extensions

> **Development design specification.** This document describes intended long-term architecture beyond the frozen `development` contract. Examples and requirements may exceed the current implementation. See the [current implementation guide](../extensions.md) before using an API.


## Development alignment

Gungnir is under active development. No 1.0 compatibility contract is frozen; this document may describe intended future architecture, but current implementation guides and executable tests remain authoritative.

Gungnir extensions add services, runtime integrations, native bindings, and application capabilities without introducing a second application lifecycle.

# Package boundary

A package is a distributable collection of code and modules.

A module is a source-language compilation unit.

Do not use those terms interchangeably.

# Providers

Extensions integrate through the normal application Provider lifecycle.

A provider may register services, boot after registrations are complete, participate in readiness, and release resources during shutdown.

# Package metadata

Package metadata should include:

~~~text
stable package name
package version
Gungnir compatibility requirement
optional capabilities
~~~

Compatibility expressions are meaningful only when a version-range grammar and checker are implemented.

Until then they are descriptive.

# Native extensions

Native C++ libraries remain valid extension mechanisms.

A native extension may expose:

- Provider;
- runtime service;
- database/cache/storage adapter;
- compiler/native binding metadata;
- explicit framework plugin.

Gungnir does not require a proprietary archive format.

# Gungnir source modules

A package may also provide Gungnir modules when package/module resolution supports them.

Package modules should have stable roots that do not collide with application modules.

# Compiler bindings

A native function/type exposed to Gungnir source must register explicit semantic metadata:

~~~text
Gungnir-visible name
parameter types
logical result type
async behavior
member/static behavior
lifetime constraints
lowering/native binding identity
~~~

The compiler must not accept arbitrary unknown C++ text as validated Gungnir.

# Plugin loading

Loading native plugins executes code with application privileges.

Discovery/loading must therefore be explicit.

The framework should not automatically execute every library found in a directory.

# Dependency injection

Packages may register container services using the same lifetimes and scope rules as application services.

# Lifecycle

Extensions use the same:

~~~text
register
boot
ready
shutdown
~~~

lifecycle as the rest of the application.

There is no independent plugin startup system.

# Compatibility

A package must not claim compatibility solely because it compiles against one internal header revision.

Source-language, runtime ABI, and native API compatibility are separate concerns.

# Security

Native extensions are trusted code.

Package installation and loading should be treated like adding application executable code.

# Design rule

~~~text
one application lifecycle
explicit package metadata
explicit native bindings
no automatic execution of discovered libraries
modules and packages remain distinct concepts
~~~

