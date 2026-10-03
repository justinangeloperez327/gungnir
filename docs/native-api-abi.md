# Native API and ABI Stability

> **Status: Development.** Native C++ source APIs and binary ABI are actively evolving. The verification infrastructure remains in place, but no 1.0 source/ABI compatibility promise is made yet.

## Development metadata

Installed consumers can include:

```cpp
#include <gungnir/version.hpp>
```

Development builds expose:

```text
release identity          development
internal CMake version    0.0.0
release channel           development
native API contract       development
native ABI epoch          0
```

The numeric `0.0.0` value exists only for CMake/package machinery.

## Installed package metadata

The CMake package exposes:

```cmake
Gungnir_VERSION
Gungnir_RELEASE_VERSION
Gungnir_RELEASE_CHANNEL
Gungnir_VERSION_MAJOR
Gungnir_VERSION_MINOR
Gungnir_VERSION_PATCH
Gungnir_NATIVE_API_CONTRACT
Gungnir_NATIVE_ABI_EPOCH
```

Development consumers should normally use:

```cmake
find_package(Gungnir CONFIG REQUIRED)
```

Do not request a stable compatibility version from a development build.

## Source compatibility

The public headers and imported targets are continuously compiled and exercised by package-consumer tests:

```text
gungnir::gungnir
gungnir::orm
gungnir::language
```

This catches accidental breakage, but it does not freeze the API before 1.0.

Compatibility-friendly changes are preferred. When a public API is replaced, use deprecation and migration guidance where practical.

## Binary ABI

Development ABI epoch is:

```text
0
```

Epoch 0 is explicitly unstable. Native applications should be rebuilt whenever the development framework changes.

Binary compatibility also depends on operating system, architecture, compiler ABI, standard library ABI, runtime model, dependencies, and build features.

## Shared-library verification

Linux and Windows shared builds remain part of CI. External consumer projects verify that installed shared packages link and execute correctly.

Linux exported-symbol manifests remain useful for review, but they are not a frozen ABI list during development.

## Generated C++

Generated application C++ is not a native API or ABI surface. Always regenerate/rebuild it with the matching compiler/runtime.

## 1.0 rule

Native API contract `1.0` and ABI epoch `1` will be assigned only during the final completeness/release audit, together with package 1.0.0 and the stable compiler contracts.

## Implementation references

- [Development Status](development-status.md)
- [cmake/version.hpp.in](../cmake/version.hpp.in)
- [include/gungnir/api.hpp](../include/gungnir/api.hpp)
- [tests/public_api_contract.cpp](../tests/public_api_contract.cpp)
- [tests/install_smoke](../tests/install_smoke)
- [.github/workflows/native-api-abi.yml](../.github/workflows/native-api-abi.yml)
