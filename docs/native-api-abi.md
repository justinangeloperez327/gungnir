# Native API and ABI Stability

Version 1.0.0 publishes native source contract `1.0` and ABI epoch `1` for the retained public APIs.

## Metadata

Include `<gungnir/version.hpp>` to read package version `1.0.0`, release channel `stable`, native API contract `1.0` and native ABI epoch `1`.

The installed CMake package exports `Gungnir_VERSION`, `Gungnir_RELEASE_VERSION`, `Gungnir_RELEASE_CHANNEL`, version components, `Gungnir_NATIVE_API_CONTRACT` and `Gungnir_NATIVE_ABI_EPOCH`. Consumers can use:

```cmake
find_package(Gungnir 1.0 CONFIG REQUIRED)
```

CMake package compatibility uses the same major version. Public headers and installed `gungnir::gungnir`, `gungnir::orm` and `gungnir::language` targets are checked in CI. Optional adapters expose their own imported targets and dependency requirements.

## Source and binary compatibility

Compatible fixes/additions retain the 1.x native source contract. Breaking source changes require a new major contract and upgrade guidance. Changes that break the supported binary line require a new ABI epoch.

ABI epoch alone does not guarantee portability across platforms or toolchains. Binary compatibility also requires matching architecture, compiler ABI, standard library ABI, runtime model, dependencies and build features. Linux/Windows shared-package consumers and Linux exported-symbol manifests remain verification evidence.

Generated application C++ is an implementation artifact. Regenerate and rebuild it with the matching compiler/runtime rather than depending on generated names or layouts.

See [Release Scope](release-v1.md), [Stability](stability.md), [Upgrading](upgrading.md), [native API tests](../tests/public_api_contract.cpp) and [shared-package CI](../.github/workflows/native-api-abi.yml).
