#!/usr/bin/env python3
"""Validate Gungnir's release identity consistency contract."""

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


def main() -> int:
    errors: list[str] = []

    cmake = read("CMakeLists.txt")
    spec = read("include/gungnir/language/spec.hpp")
    native = read("cmake/version.hpp.in")
    package = read("cmake/GungnirConfig.cmake.in")
    readme = read("README.md")

    require(
        errors,
        "project(gungnir VERSION 1.0.0 LANGUAGES CXX)" in cmake,
        "CMake package version must be 1.0.0",
    )
    require(
        errors,
        'set(GUNGNIR_RELEASE_CHANNEL "stable")' in cmake,
        "GUNGNIR_RELEASE_CHANNEL must match the 1.0 release contract",
    )
    require(
        errors,
        'set(GUNGNIR_VERSION_FULL "1.0.0")' in cmake,
        "GUNGNIR_VERSION_FULL must match the 1.0 release contract",
    )
    require(
        errors,
        "SameMajorVersion" in cmake,
        "Development CMake package compatibility must be SameMajorVersion",
    )

    for key in (
        'language_version = "1.0"',
        'compiler_contract_version = "1.0"',
        'diagnostic_contract_version = "1.0"',
        "structured_profile_feature_frozen = true",
        "compiler_compatibility = Compatibility::stable",
    ):
        require(errors, key in spec, f"Language release metadata missing: {key}")

    require(
        errors,
        'native_api_contract_version = "1.0"' in native,
        "Native API contract must match the 1.0 release contract",
    )
    require(
        errors,
        "GUNGNIR_NATIVE_ABI_EPOCH 1" in native,
        "Native ABI epoch must be 1",
    )
    require(
        errors,
        'Gungnir_NATIVE_API_CONTRACT "1.0"' in package,
        "Installed CMake package native API contract must match the 1.0 release contract",
    )
    require(
        errors,
        'Gungnir_NATIVE_ABI_EPOCH "1"' in package,
        "Installed CMake package ABI epoch must be 1",
    )

    require(
        errors,
        "Project status: **Stable 1.0.0**" in readme,
        "README must identify the stable release",
    )
    require(
        errors,
        "See the release scope and known limitations" in readme,
        "README must explain the 1.0 release rule",
    )

    forbidden_current = (
        "Current release candidate:",
        "1.0.0-rc.1",
        "Gungnir 1.0 release candidate",
        "feature-frozen for the 1.0",
        "Phase 16 benchmarks",
        "the 0.9 language",
    )

    current_files = [
        ROOT / "README.md",
        ROOT / "CONTRIBUTING.md",
        ROOT / "SUPPORT.md",
        ROOT / "SECURITY.md",
    ]
    current_files.extend(sorted((ROOT / "docs").rglob("*.md")))

    for path in current_files:
        if path.name == "upgrading.md":
            continue

        text = path.read_text(encoding="utf-8")

        for value in forbidden_current:
            if value in text:
                errors.append(
                    f"{path.relative_to(ROOT)} contains premature release claim: {value}"
                )

    # Historical release/version records are allowed only in changelog and
    # migration/upgrading context.
    changelog = read("CHANGELOG.md")
    require(
        errors,
        "## Unreleased" in changelog,
        "CHANGELOG must keep active work under Unreleased",
    )

    if errors:
        print("Release consistency contract failed:", file=sys.stderr)

        for error in errors:
            print(f" - {error}", file=sys.stderr)

        return 1

    print("Gungnir release consistency contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
