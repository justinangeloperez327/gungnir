# Stability

> **Design specification.** This document defines the long-term compatibility model that extends the current [1.0 stability contract](../stability.md). The current implementation guide is authoritative for shipped guarantees.

Gungnir has entered the 1.x compatibility line.

The 1.0 release candidate freezes the structured language, compiler semantic contract, diagnostic contract, and representative native C++ source API intended for stable 1.0.

Future evolution should be deliberate, versioned, and separated by compatibility surface rather than treating the framework as one undifferentiated contract.

# Stability layers

Gungnir has several compatibility surfaces:

~~~text
package/release identity
Gungnir source language
compiler semantic contract
diagnostic contract
application-facing framework API
compiler CLI
generated C++ representation
native runtime C++ source API
native binary ABI epoch
package/plugin API
database/storage adapter contracts
~~~

These layers do not need identical compatibility guarantees.

# Package versioning

Stable Gungnir releases follow semantic versioning.

For the 1.x line:

~~~text
1.x patch
  bug/security/portability fixes
  no intentional stable source break

1.x minor
  backward-compatible features
  deprecations may be introduced

2.0
  source-incompatible contract changes may be introduced
  with explicit migration guidance
~~~

Prerelease identifiers such as `1.0.0-rc.1` represent candidate builds of the numeric package version.

# Source language

Canonical source-language behavior is defined by:

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

The 1.0 structured language is feature-frozen at the RC/stable boundary.

After 1.0, accepted syntax and defined semantic behavior should remain source-compatible throughout the 1.x line unless preserving prior behavior would create a material correctness or security defect.

New syntax should normally be additive and must have defined parser, semantic, validated-AST, lowering, diagnostics, and cross-compiler behavior before entering a stable contract.

# Compiler semantic contract

A syntactically accepted program is not enough to define compatibility.

The compiler contract also includes:

- name resolution;
- type checking;
- overload/call resolution;
- control-flow validation;
- framework declaration semantics;
- `--check` behavior;
- deterministic lowering inputs;
- stable diagnostic identity where documented.

Compiler changes should not silently reinterpret valid 1.x programs.

# Diagnostic contract

Diagnostics have a separate compatibility surface.

Stable diagnostic codes used by tooling or documented workflows should not be renumbered or repurposed casually.

Message wording may improve when meaning remains equivalent, but machine-facing codes and source ranges should remain deterministic where the contract requires them.

# Application-facing framework API

Canonical framework behavior is defined by concept-specific current guides such as:

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

Stable framework behavior should evolve additively during 1.x.

A feature appearing in a design document does not make it part of the shipped contract. Current implementation guides and executable tests remain authoritative.

# Native runtime source API

Headers under `include/gungnir` form the native integration surface.

The 1.x native source API contract is versioned separately from the source language and is protected by compile-time contract tests plus installed-package consumer tests.

Stable source-compatible evolution should prefer:

- additive types/functions;
- overloads that do not create ambiguity;
- new optional configuration fields with compatible defaults;
- deprecation before removal;
- migration guidance for renamed or superseded APIs.

# Native binary ABI

Native binary compatibility is tracked by an explicit ABI epoch.

The 1.x line uses ABI epoch 1.

Epoch equality is necessary but not sufficient. Binary compatibility also depends on:

- operating system;
- architecture;
- compiler ABI;
- standard library ABI;
- runtime library model;
- native dependency ABI;
- relevant build features.

Gungnir should not claim universal cross-toolchain C++ ABI compatibility.

# Generated C++

Generated C++ is not a stable public source or binary API.

Projects must regenerate and rebuild generated code with the matching compiler/runtime package.

Applications should not depend directly on generated namespaces, helper names, internal class layout, or emitter spelling.

Generated output may change within 1.x when observable source-language/runtime behavior remains compatible.

# Runtime/compiler compatibility

The compiler and runtime carry explicit compatibility metadata.

Generated code must fail early rather than silently compile against an incompatible runtime contract.

The version relationship should be machine-readable and validated by release/package CI.

# Deprecation

Stable 1.x source APIs should normally pass through a deprecation period before removal.

A typical lifecycle is:

~~~text
introduce replacement
  -> document migration
  -> mark old API deprecated
  -> preserve compatibility through 1.x where practical
  -> remove in the next appropriate major version
~~~

Faster removal is acceptable when:

- a security issue requires it;
- prior behavior cannot be made safe;
- preserving compatibility would maintain materially incorrect semantics.

# Capability claims

A type, stub, branch, design document, or passing compile is not proof that a feature is production-ready.

Documentation should distinguish:

~~~text
implemented
stable
experimental
partial
planned
unsupported
~~~

The current implementation guide wins over aspirational design text.

# Concurrency claims

An API is not thread-safe, coroutine-safe, cancellation-safe, or distributed-safe merely because it compiles in concurrent code.

Concurrency guarantees must be explicit and backed by implementation/tests.

# Backend portability

A Gungnir API may be portable while a backend lacks a capability.

Backend documentation must identify unsupported operations and capability differences clearly.

Where practical, unsupported selected-backend operations should fail during validation/planning rather than during production execution.

# Plugins and packages

Extension/package compatibility should declare:

- supported Gungnir package range;
- required native API contract;
- required ABI epoch when shipping native binaries;
- platform/toolchain constraints;
- optional backend/runtime dependencies.

Binary extensions must not infer compatibility solely from package version.

# Release candidates

Release candidates freeze the intended stable contract.

During an RC, acceptable changes are primarily:

- release-blocking correctness fixes;
- security fixes;
- portability fixes;
- packaging/install fixes;
- deterministic diagnostic fixes;
- documentation corrections;
- performance fixes that preserve behavior;
- release/test automation fixes.

An RC is not a normal feature-development window.

# Documentation authority

When design text conflicts with current implementation documentation:

~~~text
current implementation guide
    wins over
design specification
~~~

Historical migration/changelog references should remain when they explain how contracts evolved.

# Design rule

~~~text
stable contracts evolve deliberately
compatibility surfaces are versioned separately
generated C++ remains rebuildable implementation detail
native ABI claims stay scoped to real C++ ABI boundaries
documentation and executable contracts must agree
~~~
