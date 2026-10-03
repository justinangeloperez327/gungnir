# Changelog

This changelog records user-visible contract milestones; Git history remains the detailed implementation record.

## [1.0.0-rc.1] - 2026-10-03

### 1.0 contract freeze

- promoted the structured language contract to 1.0;
- promoted compiler semantics and diagnostic contracts to 1.0;
- marked the structured compiler compatibility level stable;
- promoted the native C++ source API contract to 1.0;
- advanced the native ABI epoch to 1;
- switched installed CMake compatibility to the 1.x same-major policy.

### Release-candidate hardening

- added a release-candidate contract checker and dedicated RC package workflow;
- made CLI/compiler/package identity prerelease-aware;
- made release tag validation require the source-tree prerelease marker;
- verified installed package consumers and benchmark smoke from the RC package;
- froze Phase 20 as promotion-only work.

### Maturity baseline

The RC incorporates runtime lifecycle correctness, database/ORM correctness, framework semantic completion, security hardening, production resilience, performance baselines, native API/ABI stabilization, and documentation/ecosystem readiness completed after 0.9.0.

## [0.9.0] - 2026-10-02

### Compiler stabilization

- froze the structured language/compiler contract at 0.9;
- made the structured compiler canonical;
- established dedicated structural C++ IR boundaries;
- completed semantic/type-system validation required by the 0.9 profile;
- made `gungnirc --check` authoritative through validated semantics;
- hardened diagnostics/source mapping;
- added fuzzing and compiler robustness gates;
- added GCC, Clang, and MSVC conformance with byte-identical structured output.

### Runtime and framework maturity

- hardened request/coroutine lifecycle and graceful shutdown;
- strengthened database and ORM correctness;
- completed the Phase 13 framework semantic contract;
- hardened HTTP/session/application security boundaries;
- added production resilience, health, retry, and overload admission controls;
- established reproducible compiler, HTTP/routing, and ORM benchmarks;
- stabilized representative native C++ API signatures and native ABI epoch metadata.

### Packaging

- Linux x86_64 portable package;
- Windows x86_64 portable package;
- Windows setup executable;
- installed CMake package targets;
- compiler and CLI contract/version verification.

## Unreleased

- final 1.0.0 promotion only; no new features are planned between RC1 and stable unless required by a release-blocking correction.
