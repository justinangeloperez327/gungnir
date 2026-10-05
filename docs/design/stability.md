# Stability

> **Future design specification.** This document defines the compatibility model Gungnir should adopt when the framework eventually reaches stable 1.0. The current [stability guide](../stability.md) remains authoritative for development behavior.

Gungnir is still under active feature-completion development.

## Development rule

No stable source, compiler, diagnostic, native API, or ABI version is frozen yet.

Current development work should still favor compatibility-friendly evolution, clear migration paths, and executable regression contracts, but necessary architectural changes remain allowed until the final completeness audit.

## Stability layers

Gungnir tracks several independent compatibility surfaces:

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

These surfaces do not need identical compatibility guarantees.

## Eventual stable versioning

When Gungnir becomes complete, stable releases should follow semantic versioning.

The intended stable policy is:

~~~text
1.x patch
  correctness/security/portability fixes
  no intentional stable source break

1.x minor
  backward-compatible features
  deprecations may be introduced

2.0
  source-incompatible contract changes may be introduced
  with explicit migration guidance
~~~

Development builds before that point are identified as `development`, not as artificial prerelease milestones.

## Source language

Canonical source-language behavior is defined by the current grammar, type, expression, statement, function, async, module, AST, semantic, validated-AST and transpiler documentation.

Before stable 1.0, new syntax is allowed when it is needed for framework completeness and is implemented end-to-end through:

~~~text
lexer
-> parser
-> syntax AST
-> semantics
-> validated AST
-> typed C++ IR
-> emitter
~~~

Raw source rewriting must not become the canonical implementation path for new structured features.

## Compiler semantic contract

Compiler development must preserve correctness invariants while the language evolves.

The compiler contract includes:

- name resolution;
- type checking;
- overload/call resolution;
- control-flow validation;
- framework declaration semantics;
- `--check` behavior;
- deterministic lowering;
- diagnostic identity where tooling depends on it.

A deliberate semantic change must update tests and documentation in the same change.

## Diagnostics

Diagnostic codes used by tooling should remain stable where practical even during development.

A code must not be silently repurposed for unrelated meaning. Message wording and source-range quality may improve as long as machine-facing behavior stays coherent.

## Framework API

Current framework guides define implemented behavior.

Design documents may propose richer APIs, but proposed behavior is not considered implemented until code, tests, and current documentation agree.

## Native source API

Headers under `include/gungnir` form the native integration surface.

During development, source compatibility is preferred but not guaranteed. The package-consumer and public-API tests exist to make changes visible, not to freeze the surface prematurely.

## Native ABI

Development ABI epoch is 0.

ABI epoch 0 means unstable development binary compatibility. Native applications should rebuild when the framework changes.

When stable 1.0 is finally assigned, the native ABI epoch should advance to 1 in the same coordinated release change.

## Generated C++

Generated C++ remains an implementation artifact and is not a stable public source/ABI surface.

Projects must regenerate/rebuild generated code with the matching development compiler/runtime.

## Deprecation

During development, deprecation is still preferred when a public API has real external users and a safe migration path exists.

After stable 1.0, source-incompatible removal should normally wait for an appropriate major release unless security/correctness requires faster action.

## Capability claims

A type, stub, parser node, design document, or successful compilation is not proof of feature completion.

Documentation should distinguish:

~~~text
implemented
complete
partial
planned
unsupported
~~~

The current implementation guide wins over design text.

## Concurrency claims

Thread-safety, coroutine-safety, cancellation-safety, and distributed-safety must be explicit and tested.

## Backend portability

Portable APIs must document backend capability differences.

Unsupported operations should fail as early and explicitly as practical.

## Plugins and packages

Extension compatibility should eventually declare:

- supported Gungnir stable range;
- required native API contract;
- ABI epoch for native binaries;
- platform/toolchain constraints;
- optional backend/runtime dependencies.

During development, extensions should pin exact revisions or development artifacts.

## Final 1.0 transition

Gungnir should move from `development` directly to `1.0.0` only after the framework completeness gate and release audit are green.

At that point the project can freeze:

- language contract 1.0;
- compiler contract 1.0;
- diagnostic contract 1.0;
- native API contract 1.0;
- native ABI epoch 1;
- stable compatibility policy.

## Design rule

~~~text
development may evolve
correctness stays enforced
canonical architecture stays consistent
stable promises wait until completeness
generated C++ remains rebuildable implementation detail
~~~
