#!/usr/bin/env python3
"""Validate the source tree as coherent pre-1.0 development."""

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

    require(errors, core == "0.0.0", f"Development core version must be 0.0.0, got {core}")
    require(
        errors,
        prerelease == "dev",
        f"Development prerelease marker must be dev, got {prerelease!r}",
    )
    require(
        errors,
        "SameMajorVersion" not in cmake,
        "Development package must not promise SameMajorVersion compatibility",
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
    require(errors, 'language_version = "development"' in spec, "language contract is not development")
    require(
        errors,
        'compiler_contract_version = "development"' in spec,
        "compiler contract is not development",
    )
    require(
        errors,
        'diagnostic_contract_version = "development"' in spec,
        "diagnostic contract is not development",
    )
    require(
        errors,
        "structured_profile_feature_frozen = false" in spec,
        "structured profile must remain open during feature completion",
    )
    require(
        errors,
        "compiler_compatibility = Compatibility::experimental" in spec,
        "pre-1.0 compiler compatibility must remain experimental",
    )

    native = read("cmake/version.hpp.in")
    require(
        errors,
        "GUNGNIR_NATIVE_API_CONTRACT_MAJOR 0" in native
        and "GUNGNIR_NATIVE_API_CONTRACT_MINOR 0" in native,
        "native API contract major is not 0",
    )
    require(
        errors,
        "GUNGNIR_NATIVE_ABI_EPOCH 0" in native,
        "native ABI epoch is not 0",
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
        'Gungnir_NATIVE_API_CONTRACT "development"' in package,
        "installed CMake package native API contract is not development",
    )
    require(
        errors,
        'Gungnir_NATIVE_ABI_EPOCH "0"' in package,
        "installed CMake package ABI epoch is not 0",
    )

    readme = read("README.md")
    require(
        errors,
        "pre-1.0 development" in readme,
        "README does not identify the tree as pre-1.0 development",
    )

    changelog = read("CHANGELOG.md")
    require(
        errors,
        "## Unreleased" in changelog,
        "CHANGELOG is missing Unreleased",
    )

    stability = read("docs/stability.md")
    require(
        errors,
        "pre-1.0" in stability,
        "stability guide is not marked pre-1.0",
    )

    rc_doc = read("docs/release-candidate.md")
    require(
        errors,
        "not a release candidate" in rc_doc,
        "release-candidate guide does not explain development status",
    )

    if errors:
        print("Development contract failed:", file=sys.stderr)

        for error in errors:
            print(f" - {error}", file=sys.stderr)

        return 1

    print(f"Gungnir {release} development contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
