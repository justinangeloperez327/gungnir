# Development Status

> **Project status: Development.** Gungnir does not have a 1.0 release candidate. Version 1.0 will be assigned only after the framework completeness gate is fully satisfied.

## Release rule

Gungnir reaches 1.0 because the framework is complete, not because a phase number or calendar date was reached.

The development sequence is:

```text
development
    -> feature completeness
    -> framework consistency
    -> correctness
    -> production readiness
    -> ecosystem readiness
    -> final release audit
    -> Gungnir 1.0.0
```

No intermediate development milestone is treated as a public compatibility promise.

## Machine-readable development contract

Current builds report:

```text
package_version=development
language_version=development
compiler_contract=development
diagnostic_contract=development
structured_feature_freeze=false
compatibility=experimental
```

The numeric CMake project version is `0.0.0` only because CMake requires a numeric value for package/build machinery. It is not a public Gungnir release number.

Native package metadata uses:

```text
native API contract = development
native ABI epoch    = 0
release channel     = development
```

## What remains protected

Removing premature version promises does **not** remove engineering discipline.

Development builds still preserve:

- compiler correctness and deterministic output tests;
- GCC, Clang, and MSVC conformance;
- diagnostics/source mapping tests;
- fuzz/sanitizer gates;
- runtime lifecycle guarantees;
- database/ORM correctness tests;
- security hardening tests;
- production resilience tests;
- performance baselines;
- native package/install consumer tests;
- documentation integrity checks.

These are implementation guarantees under active development, not 1.0 compatibility commitments.

## Completeness before 1.0

The final release requires end-to-end completion and consistency across:

- compiler/language pipeline;
- models and ORM;
- relationships;
- migrations;
- routing/controllers/model binding;
- middleware/request/response;
- validation;
- authentication/authorization;
- sessions/CSRF;
- events/listeners;
- jobs/queues/scheduler;
- notifications/mail;
- views/storage/cache;
- all supported database backends;
- async/runtime/network behavior;
- security and production operation;
- CLI/generators/project DX;
- testing;
- packaging/installers;
- documentation;
- performance/stress/fuzz coverage;
- public API consistency.

A feature is not complete merely because a class, interface, parser node, or stub exists. It must work end-to-end with tests and current documentation.

## Consistency rule

All structured language features should follow the same compiler architecture:

```text
source
-> lexer
-> parser
-> syntax AST
-> semantics
-> validated AST
-> typed C++ IR
-> C++23 emitter
```

Compatibility/source-edit lowering must remain isolated from the canonical structured path.

Framework APIs should follow one consistent naming and behavior model wherever C++ permits it.

## When 1.0 is assigned

Only the final release audit changes the project from `development` to `1.0.0`.

At that point, in one coordinated change:

- package version becomes `1.0.0`;
- language contract becomes `1.0`;
- compiler contract becomes `1.0`;
- diagnostic contract becomes `1.0`;
- native API contract becomes `1.0`;
- ABI epoch becomes `1`;
- feature freeze becomes true;
- compatibility becomes stable;
- release documentation is published.

Until then, Gungnir remains **Development**.
