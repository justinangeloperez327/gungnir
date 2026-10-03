# Native API and ABI Stability

> **Status: pre-1.0 development.** The native C++ source API and binary ABI are not frozen while Gungnir completes its 1.0 framework surface.

## Current identity

```text
release version           0.0.0-dev
CMake package version     0.0.0
native API contract       development
native ABI epoch          0
```

The generated native version header exposes package version macros/constants, `gungnir::version`, `gungnir::version_prerelease`, `gungnir::native_api_contract_version`, and `gungnir::native_abi_epoch`.

The installed CMake package exposes the corresponding `Gungnir_RELEASE_VERSION`, `Gungnir_NATIVE_API_CONTRACT`, and `Gungnir_NATIVE_ABI_EPOCH` values.

## Source API policy before 1.0

Public native APIs may still be extended or corrected to complete the intended framework behavior. Changes should preserve source compatibility where practical, but no 1.x compatibility promise exists yet.

Headers and imported targets must remain internally coherent and covered by the public API contract tests.

## ABI epoch 0

Epoch `0` explicitly means the binary contract is unstable. Applications and generated C++ should be rebuilt against the matching Gungnir package.

Even after a stable ABI epoch is assigned, C++ binary compatibility will still depend on operating system, architecture, compiler ABI, standard library ABI, runtime model, dependency ABI, and relevant feature/build options.

## 1.0 transition

The native API contract should become `1.0` and the ABI epoch should advance from `0` only when the full intended 1.0 feature surface is complete and the representative public native surface has been audited.

The transition must include public-header tests, installed-package consumer tests, supported compiler/platform checks, documentation review, and release packaging verification.

See [Stability](stability.md), [Pre-1.0 Development Status](release-candidate.md), and [Upgrading](upgrading.md).
