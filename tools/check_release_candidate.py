#!/usr/bin/env python3
"""Validate the source tree as a coherent Gungnir 1.0 release candidate."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(errors: list[str], condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


def match(pattern: str, text: str, label: str, errors: list[str]) -> str:
    found = re.search(pattern, text, re.MULTILINE)

    if not found:
        errors.append(f"Unable to read {label}")
        return ""

    return found.group(1)


def main() -> int:
    errors: list[str] = []

    cmake = read("CMakeLists.txt")
    core = match(
        r"project\(gungnir VERSION (\d+\.\d+\.\d+) LANGUAGES CXX\)",
        cmake,
        "CMake project version",
        errors,
    )
    prerelease = match(
        r'set\(GUNGNIR_VERSION_PRERELEASE "([^"]*)"\)',
        cmake,
        "GUNGNIR_VERSION_PRERELEASE",
        errors,
    )

    release = core + (f"-{prerelease}" if prerelease else "")

    require(errors, core == "1.0.0", f"RC core version must be 1.0.0, got {core}")
    require(
        errors,
        re.fullmatch(r"rc\.\d+", prerelease) is not None,
        f"RC prerelease marker must be rc.N, got {prerelease!r}",
    )
    require(
        errors,
        "SameMajorVersion" in cmake,
        "1.x installed CMake package must use SameMajorVersion compatibility",
    )
    require(
        errors,
        'CPACK_PACKAGE_VERSION "${PROJECT_VERSION}"' in cmake,
        "CPack internal package version must remain numeric",
    )
    require(
        errors,
        'gungnir-v${GUNGNIR_VERSION_FULL}-windows-x86_64-setup' in cmake,
        "Windows installer filename must include the full RC identity",
    )

    spec = read("include/gungnir/language/spec.hpp")
    require(errors, 'language_version = "1.0"' in spec, "language contract is not 1.0")
    require(
        errors,
        'compiler_contract_version = "1.0"' in spec,
        "compiler contract is not 1.0",
    )
    require(
        errors,
        'diagnostic_contract_version = "1.0"' in spec,
        "diagnostic contract is not 1.0",
    )
    require(
        errors,
        "structured_profile_feature_frozen = true" in spec,
        "structured 1.0 profile must remain feature-frozen during RC",
    )
    require(
        errors,
        "compiler_compatibility = Compatibility::stable" in spec,
        "1.0 RC compiler compatibility must be stable",
    )

    native = read("cmake/version.hpp.in")
    require(
        errors,
        "GUNGNIR_NATIVE_API_CONTRACT_MAJOR 1" in native
        and "GUNGNIR_NATIVE_API_CONTRACT_MINOR 0" in native,
        "native API contract is not 1.0",
    )
    require(
        errors,
        "GUNGNIR_NATIVE_ABI_EPOCH 1" in native,
        "native ABI epoch is not 1",
    )
    require(
        errors,
        "@GUNGNIR_VERSION_FULL@" in native,
        "installed native version header does not carry full RC identity",
    )

    package = read("cmake/GungnirConfig.cmake.in")
    require(
        errors,
        'Gungnir_RELEASE_VERSION "@GUNGNIR_VERSION_FULL@"' in package,
        "installed CMake package does not expose full release version",
    )
    require(
        errors,
        'Gungnir_NATIVE_API_CONTRACT "1.0"' in package,
        "installed CMake package native API contract is not 1.0",
    )
    require(
        errors,
        'Gungnir_NATIVE_ABI_EPOCH "1"' in package,
        "installed CMake package ABI epoch is not 1",
    )

    release_workflow = read(".github/workflows/release.yml")
    require(
        errors,
        "GUNGNIR_VERSION_PRERELEASE" in release_workflow,
        "release workflow does not validate the source prerelease marker",
    )
    require(
        errors,
        "language_version=1.0" in release_workflow
        and "compiler_contract=1.0" in release_workflow
        and "diagnostic_contract=1.0" in release_workflow,
        "release workflow does not verify 1.0 compiler contracts",
    )
    require(
        errors,
        "compatibility=stable" in release_workflow,
        "release workflow does not verify stable compiler compatibility",
    )

    readme = read("README.md")
    require(
        errors,
        f"Current release candidate: **v{release}**" in readme,
        f"README does not identify v{release} as the current release candidate",
    )

    changelog = read("CHANGELOG.md")
    require(
        errors,
        f"## [{release}]" in changelog,
        f"CHANGELOG is missing {release}",
    )

    stability = read("docs/stability.md")
    require(
        errors,
        "Gungnir 1.0 release candidate" in stability,
        "stability guide is not marked as the 1.0 release candidate contract",
    )

    rc_doc = read("docs/release-candidate.md")
    require(
        errors,
        release in rc_doc,
        "release-candidate guide does not name the current RC",
    )

    for path in sorted((ROOT / "docs").glob("*.md")):
        text = path.read_text(encoding="utf-8")
        if "pre-1.0" in text:
            errors.append(
                f"{path.relative_to(ROOT)} still carries a pre-1.0 status/reference"
            )

    if errors:
        print("Release-candidate contract failed:", file=sys.stderr)

        for error in errors:
            print(f" - {error}", file=sys.stderr)

        return 1

    print(f"Gungnir {release} release-candidate contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
