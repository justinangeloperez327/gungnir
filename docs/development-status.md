# Release Status

Gungnir 1.0.0 is the packaged application framework described in the [release guide](release-v1.md). This release assigns stable compatibility to the existing published scope; it does not claim every long-term design proposal is implemented.

## Machine-readable contracts

```text
package_version=1.0.0
language_version=1.0
compiler_contract=1.0
diagnostic_contract=1.0
structured_feature_freeze=true
compatibility=stable
native API contract=1.0
native ABI epoch=1
release channel=stable
```

Package, language, compiler, diagnostics and native metadata change together. Correctness, compiler conformance, diagnostics/source mapping, fuzz/sanitizer, runtime, ORM, security, performance, installed-package and documentation gates remain enabled.

## Scope and further work

The merged application services, resource authorization, Redis authentication persistence, views and dependency scopes are included. Generated mail/notification delivery integration remains follow-up work. Optional adapters require explicit builds and service provisioning; the core installer does not include their dependencies. See the [release scope and limitations](release-v1.md) before selecting features for an application.

The earlier [completeness audit](framework-completeness-audit.md) is a historical development baseline. Current acceptance evidence is indexed in [engineering audits](../engineering/README.md).

## Evolution

Preserve the canonical `.gnr` → syntax AST → validated AST → typed C++ IR → C++23 pipeline. Patch releases correct compatible behavior; minor releases may add compatible capabilities. Breaking language/native source changes require a new major contract and upgrade guidance. Generated C++ must be regenerated and rebuilt with the matching SDK.
