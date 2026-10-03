# Native API and ABI Stability

> **Status: Gungnir 0.9 pre-1.0 contract.** Phase 17 stabilizes the native C++ consumption surface for the 0.9 line. It does not claim that binaries produced by different C++ toolchains or standard libraries are interchangeable.

## Contract identifiers

Installed consumers can include:

```cpp
#include <gungnir/version.hpp>
```

The header exposes:

- `GUNGNIR_VERSION_MAJOR`, `GUNGNIR_VERSION_MINOR`, and `GUNGNIR_VERSION_PATCH`;
- `GUNGNIR_VERSION_STRING`;
- `gungnir::version`;
- `gungnir::native_api_contract_version`;
- `gungnir::native_abi_epoch`.

For 0.9.0:

```text
package version          0.9.0
native API contract      0.9
native ABI epoch         0
```

The generated CMake package exports the same metadata:

```cmake
Gungnir_VERSION
Gungnir_VERSION_MAJOR
Gungnir_VERSION_MINOR
Gungnir_VERSION_PATCH
Gungnir_NATIVE_API_CONTRACT
Gungnir_NATIVE_ABI_EPOCH
```

## Native source compatibility

The 0.9 native API contract is the reviewed C++ surface exposed by installed public headers and the stable imported CMake targets:

```text
gungnir::gungnir
gungnir::orm
gungnir::language
```

Within the 0.9 patch line, an ordinary maintenance change should not silently:

- remove or rename a stabilized public type;
- change the signature of a stabilized public method/function;
- change a public alias relied on by consumers;
- remove an installed public header;
- rename an imported CMake target;
- change documented default runtime option types without an explicit compatibility decision.

The primary CI gate contains compile-time signature assertions for representative high-value APIs including `Application`, `Router`, `Response`, compiler entry points, ORM compilation, and runtime options.

This is not a statement that every incidental helper in every header is permanently frozen. The stabilization contract should expand intentionally as APIs graduate toward 1.0.

## Pre-1.0 CMake version matching

Gungnir uses `SameMinorVersion` for its generated `GungnirConfigVersion.cmake` while the package major version is zero.

A consumer requesting:

```cmake
find_package(Gungnir 0.9 CONFIG REQUIRED)
```

may resolve a compatible 0.9 patch release, but should not silently resolve a future 0.10 release as if the native contract were unchanged.

This is stricter than a same-major policy and matches the pre-1.0 compatibility model.

## Binary ABI epoch

`native_abi_epoch` is an explicit binary compatibility epoch.

The 0.9 line uses:

```text
native ABI epoch = 0
```

Epoch equality is **necessary but not sufficient** for binary compatibility. Native C++ binaries must also use a compatible ABI environment, including:

- operating system and architecture;
- compiler family and compatible compiler ABI;
- standard library implementation and ABI mode;
- runtime library model;
- Gungnir feature/build configuration that affects linked native dependencies.

Do not link a GCC/libstdc++ Gungnir binary into an unrelated MSVC build merely because both report ABI epoch 0.

Generated Gungnir application C++ remains a build artifact and should be rebuilt with the matching Gungnir package/runtime.

## Shared-library identity

The core native libraries carry CMake `VERSION` and `SOVERSION` metadata:

- package/library version: `0.9.0`;
- shared-object version: `0` for the pre-1.0 ABI epoch.

Shared builds define `GUNGNIR_SHARED`. The public `<gungnir/api.hpp>` header provides:

- `GUNGNIR_API` for binary visibility annotations;
- `GUNGNIR_DEPRECATED(message)` for source-compatible deprecation.

Windows shared builds currently use CMake export-all-symbol support for the pre-1.0 line while public declarations progressively move behind explicit visibility annotations. This avoids pretending that symbol visibility itself is already a 1.0-final surface.

## Deprecation policy

A stabilized API should normally be deprecated before removal when a safe migration path exists.

Use:

```cpp
GUNGNIR_DEPRECATED("Use replacement() instead")
void legacy();
```

For the 0.9 line:

1. introduce the replacement;
2. preserve the old source API where practical;
3. mark the old API deprecated;
4. document the migration;
5. remove it only with an explicitly breaking contract/version decision.

Correctness or security defects may require faster removal when preserving the old behavior would be unsafe.

## Installed-package verification

Phase 17 strengthens the external package smoke test. The consumer now:

- requests Gungnir 0.9 through `find_package`;
- verifies native API and ABI CMake metadata;
- includes the installed version header;
- checks the package/native contract at compile time;
- links and calls core runtime APIs;
- invokes the installed compiler library;
- invokes ORM query compilation.

This catches packaging mistakes that in-tree tests cannot detect.

## Shared-package ABI workflow

The **Native API ABI** workflow builds and installs shared Gungnir libraries on Linux and Windows and then builds/runs the external install-smoke consumer against those installed libraries.

The Linux job also archives a demangled exported-symbol manifest for review. The manifest is observational during 0.9 rather than an automatic symbol-by-symbol freeze because compiler-generated C++ symbols are toolchain-specific.

An intentional ABI break must advance the ABI policy rather than silently changing shared-library identity.

## 1.0 transition

Before Gungnir 1.0, Phase 19 should decide which native APIs graduate from the 0.9 stabilization set into the 1.x compatibility guarantee.

At 1.0, the expected policy is:

- package SemVer becomes authoritative for native source compatibility;
- the stable native API set is documented explicitly;
- ABI epoch advances to the 1.x line;
- binary compatibility claims remain scoped by supported platform/toolchain combinations;
- removals of stable APIs require the normal major-version/deprecation process.

## Implementation references

- [cmake/version.hpp.in](../cmake/version.hpp.in)
- [include/gungnir/api.hpp](../include/gungnir/api.hpp)
- [tests/public_api_contract.cpp](../tests/public_api_contract.cpp)
- [tests/install_smoke](../tests/install_smoke)
- [.github/workflows/native-api-abi.yml](../.github/workflows/native-api-abi.yml)
- [CMakeLists.txt](../CMakeLists.txt)

See also [Stability](stability.md), [Compiler Conformance](compiler-conformance.md), and [Performance Baseline](performance.md).
