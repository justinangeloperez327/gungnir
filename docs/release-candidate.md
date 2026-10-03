# Gungnir 1.0 Release Candidate

> **Current candidate: 1.0.0-rc.1.** Phase 19 freezes the contracts intended to ship as Gungnir 1.0. Phase 20 is the final promotion step, not another feature-development phase.

## Frozen RC contracts

| Contract | RC value |
| --- | --- |
| Package release | 1.0.0-rc.1 |
| Structured language | 1.0 |
| Compiler semantic contract | 1.0 |
| Diagnostic contract | 1.0 |
| Structured feature freeze | true |
| Compiler compatibility | stable |
| Native C++ source API | 1.0 |
| Native ABI epoch | 1 |
| CMake compatibility | same major version |

The accepted 1.0 structured language is the previously stabilized 0.9 profile promoted to the 1.0 compatibility contract after the correctness, semantics, security, resilience, performance, API/ABI, and documentation phases.

No new syntax or framework semantics should be added between this RC and 1.0 unless a release-blocking defect makes a contract correction necessary.

## Release-blocking criteria

The RC is not ready when any of these are red:

- primary correctness CI;
- compiler fuzz/sanitizer smoke;
- GCC/Clang/MSVC compiler conformance;
- byte-identical compiler snapshots;
- Windows installer smoke;
- Linux/Windows shared native API/ABI consumer tests;
- documentation/example contract;
- Release-mode performance baseline build/measurement;
- release-candidate package contract.

A release-blocking correctness or security defect must be fixed before promotion.

## Allowed changes during RC

Allowed:

- correctness fixes;
- security fixes;
- portability fixes;
- packaging/install fixes;
- documentation corrections;
- deterministic diagnostic/source-location fixes;
- performance fixes that preserve public behavior;
- test and release automation fixes.

Avoid:

- new language syntax;
- new framework declarations;
- public API redesign;
- semantic behavior expansion;
- undocumented ABI breaks;
- dependency/toolchain churn without a release blocker.

## RC package identity

The source tree uses numeric CMake version `1.0.0` plus prerelease marker `rc.1`.

Installed CLI output is:

```text
Gungnir 1.0.0-rc.1
Gungnir compiler 1.0.0-rc.1 (language 1.0)
```

The CMake package exposes numeric package compatibility as `1.0.0` plus `Gungnir_RELEASE_VERSION=1.0.0-rc.1`.

This split keeps CMake package-version semantics valid while preserving the full prerelease identity for users and release assets.

## ABI policy

Native API contract `1.0` and ABI epoch `1` are the first 1.x contracts.

ABI epoch equality is still not a promise that binaries from unrelated C++ ABI environments can be mixed. Platform, architecture, compiler ABI, standard library ABI, runtime model, and relevant build features must remain compatible.

Generated application C++ remains a rebuild artifact and must be regenerated/rebuilt with the matching package.

## Promotion to 1.0

Phase 20 should be deliberately small:

1. confirm every RC gate is green;
2. remove the `rc.1` prerelease marker;
3. update release-facing documentation/changelog from candidate to stable;
4. verify installed package/CLI identities are exactly `1.0.0`;
5. tag `v1.0.0`;
6. run the existing release workflow and publish assets/checksums.

Phase 20 should not introduce new features.

## Implementation references

- [Stability](stability.md)
- [Native API and ABI Stability](native-api-abi.md)
- [Performance Baseline](performance.md)
- [Production Resilience](production-resilience.md)
- [Security Hardening](security-hardening.md)
- [Testing](testing.md)
- [Upgrading](upgrading.md)
- [Release workflow](../.github/workflows/release.yml)
- [RC readiness workflow](../.github/workflows/release-candidate.yml)
