# Native API and ABI Stability

> **Status: Gungnir 1.0 release candidate.** The native C++ source API contract is 1.0 and ABI epoch is 1. Binary compatibility claims remain scoped to compatible C++ ABI environments.

## Contract identifiers

Installed consumers include:

```cpp
#include <gungnir/version.hpp>
```

For RC1:

```text
release version           1.0.0-rc.1
CMake package version     1.0.0
native API contract       1.0
native ABI epoch          1
```

The native header exposes package version macros/constants, `gungnir::version`, `gungnir::version_prerelease`, `gungnir::native_api_contract_version`, and `gungnir::native_abi_epoch`.

The installed CMake package exposes:

```cmake
Gungnir_VERSION
Gungnir_RELEASE_VERSION
Gungnir_VERSION_PRERELEASE
Gungnir_VERSION_MAJOR
Gungnir_VERSION_MINOR
Gungnir_VERSION_PATCH
Gungnir_NATIVE_API_CONTRACT
Gungnir_NATIVE_ABI_EPOCH
```

## Native source compatibility

The 1.0 native source contract covers the reviewed public surface exposed through installed headers and stable imported targets:

```text
gungnir::gungnir
gungnir::orm
gungnir::language
```

Within 1.x, ordinary compatible changes should not silently remove or rename stabilized public types, change stabilized signatures, remove installed public headers, rename imported targets, or change documented defaults in a source-breaking way.

The primary CI gate contains compile-time assertions for representative high-value APIs including `Application`, `Router`, `Response`, compiler entry points, ORM compilation, aliases, and runtime options.

## CMake version compatibility

The 1.x package uses `SameMajorVersion`.

A consumer may request:

```cmake
find_package(Gungnir 1.0 CONFIG REQUIRED)
```

The prerelease source tree has numeric CMake package version `1.0.0` and additionally exposes `Gungnir_RELEASE_VERSION=1.0.0-rc.1`.

Applications that must reject prereleases should check `Gungnir_RELEASE_VERSION` explicitly.

## Binary ABI epoch

The 1.x line uses:

```text
native ABI epoch = 1
```

Epoch equality is necessary but not sufficient. Native C++ binaries must also use compatible:

- operating system and architecture;
- compiler family/ABI;
- standard library implementation and ABI mode;
- runtime library model;
- linked native dependency ABIs;
- Gungnir feature/build configuration where it changes the binary surface.

Do not mix unrelated MSVC and GCC/libstdc++ binaries solely because both report epoch 1.

## Shared-library identity

Core shared libraries carry numeric CMake `VERSION 1.0.0` and `SOVERSION 1`.

The prerelease label belongs to package/release identity, while the ABI epoch controls the shared-object line.

Linux and Windows shared builds are installed and consumed by an external test project in the **Native API ABI** workflow. Linux also archives a demangled exported-symbol manifest for review.

## Deprecation policy

Stable APIs should normally be deprecated before removal when a safe migration path exists:

```cpp
GUNGNIR_DEPRECATED("Use replacement() instead")
void legacy();
```

For 1.x:

1. introduce the replacement;
2. preserve the stable source API where practical;
3. mark the old API deprecated;
4. document migration;
5. remove source-incompatible APIs only in an appropriate major-version change, except when preserving behavior would be unsafe.

## Generated application C++

Generated Gungnir C++ is not part of the native source/ABI promise. Regenerate and rebuild it with the matching compiler/runtime package.

## RC to stable

The 1.0 RC freezes API contract 1.0 and ABI epoch 1. Stable 1.0 should not change those values.

Phase 20 removes the prerelease marker and verifies/publishes the stable package; it should not redesign the native surface.

## Implementation references

- [cmake/version.hpp.in](../cmake/version.hpp.in)
- [include/gungnir/api.hpp](../include/gungnir/api.hpp)
- [tests/public_api_contract.cpp](../tests/public_api_contract.cpp)
- [tests/install_smoke](../tests/install_smoke)
- [.github/workflows/native-api-abi.yml](../.github/workflows/native-api-abi.yml)
- [1.0 Release Candidate](release-candidate.md)
- [CMakeLists.txt](../CMakeLists.txt)
