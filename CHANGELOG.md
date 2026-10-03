# Changelog

Gungnir follows a pre-1.0 stabilization model. This changelog records user-visible contract milestones; Git history remains the detailed implementation record.

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

### Documentation and ecosystem

- canonical documentation integrity checking;
- canonical minimal application example;
- contributor, support, security, and upgrade guidance;
- documentation CI and copyable-example validation.
