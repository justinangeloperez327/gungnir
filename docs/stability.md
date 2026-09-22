# API Stability

Gungnir is pre-stable. The current project version is 0.1.0.

## Public API

Headers under `include/gungnir` form the candidate public surface, but compatibility is not yet guaranteed. Breaking changes should still be deliberate and documented rather than incidental.

## Deprecation

Once Gungnir declares a stable 1.x API, public removals and incompatible changes should normally pass through a deprecation period unless required to correct a security defect or an API whose behavior cannot be made safe.

## Capability claims

Documentation and generated code must describe only behavior implemented by the repository. Placeholder interfaces are not evidence of a working backend, protocol, transport, provider or distributed guarantee.

## Native interoperability

Gungnir-generated code targets C++23. Native C++ remains an interoperability boundary; framework convenience must not require replacing the C++ toolchain or package ecosystem.

## Concurrency

APIs are not thread-safe or coroutine-safe merely because their types can be called from multiple contexts. Thread-safety guarantees must be documented per subsystem and backed by the implementation.

## Versioning

The CMake package currently uses the repository project version. Package/plugin compatibility metadata is descriptive until a concrete version-range grammar and compatibility checker are implemented.
