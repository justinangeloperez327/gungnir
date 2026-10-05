# Version 1.0.0 release audit

Source baseline: merged PR #183, commit `3562dadffe399bf2b06e40da635ae22bd3152b82`.
The user selected release of this application-ready scope and deferred the
remaining generated mail/notification delivery integration. Its unpublished
changes are excluded from this release.

| Contract | Release decision and evidence |
| --- | --- |
| Identity | Package 1.0.0; language/compiler/diagnostics/native API 1.0; ABI epoch 1; stable channel; frozen existing structured surface. CLI, headers, CMake consumers, source checks and platform workflows use the same identity. |
| Implementation | The baseline includes the merged model/ORM, HTTP, context, auth, services, routing, validation, background and application audits. This change does not redesign their implementations. |
| Local acceptance | Release core compiled under GCC 13. Six selected contract checks passed: native API, compiler stability, application integration, compiler CLI metadata, deterministic snapshots and public application examples. Installed native/application consumers passed. A fresh project created by the installed CLI built in Release mode and served the expected HTTP response using automatic SDK discovery. |
| CI | The baseline's six main workflows passed, including live PostgreSQL/MySQL/Redis and TLS. The release PR separately runs compiler conformance, native shared packages, security/fuzz/runtime/ORM, docs, performance, Windows installer and both portable-package checks. Publication requires all six main validation workflows to pass on the exact packaged commit. |
| Artifacts | Linux x86_64 tar.gz; Windows x86_64 zip and NSIS setup executable; SHA256SUMS.txt. Each SDK compiles an external consumer and a fresh app; Windows setup additionally verifies long PATH, installed project build and uninstall restoration. |
| Dependencies and licensing | Gungnir's MIT LICENSE and README are installed. These artifacts build the core with optional external adapters disabled. Compiler/CMake and platform runtimes are prerequisites. Optional adapter dependencies are supplied by source-build users and are not represented as bundled or verified by the core assets. |
| Publishing | Build from a pinned commit. Create its immutable version tag only after package checks and exact-commit CI succeed, then publish all assets together. Published assets are not overwritten. PR runs cannot publish. |
| Retained limitations | Typed generated mail/notification delivery remains pending. Live SQL Server/MongoDB/SMTP and external cloud/collector acceptance are not established by this release. Adapter provisioning, durability/topology and workload acceptance remain deployment-specific. Public release notes state these limits. |

The old completeness matrix is a historical baseline and is not a claim that
all long-term design capabilities have been implemented. The current published
scope is [the release guide](../docs/release-v1.md).
