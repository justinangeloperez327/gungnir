#!/usr/bin/env python3
"""Validate Gungnir's development-state consistency contract."""

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
        "project(gungnir VERSION 0.0.0 LANGUAGES CXX)" in cmake,
        "CMake development placeholder must be 0.0.0",
    )
    require(
        errors,
        'set(GUNGNIR_RELEASE_CHANNEL "development")' in cmake,
        "GUNGNIR_RELEASE_CHANNEL must be development",
    )
    require(
        errors,
        'set(GUNGNIR_VERSION_FULL "development")' in cmake,
        "GUNGNIR_VERSION_FULL must be development",
    )
    require(
        errors,
        "ExactVersion" in cmake,
        "Development CMake package compatibility must be ExactVersion",
    )

    for key in (
        'language_version = "development"',
        'compiler_contract_version = "development"',
        'diagnostic_contract_version = "development"',
        "structured_profile_feature_frozen = false",
        "compiler_compatibility = Compatibility::experimental",
    ):
        require(errors, key in spec, f"Language development metadata missing: {key}")

    require(
        errors,
        'native_api_contract_version = "development"' in native,
        "Native API contract must remain development",
    )
    require(
        errors,
        "GUNGNIR_NATIVE_ABI_EPOCH 0" in native,
        "Development native ABI epoch must remain 0",
    )
    require(
        errors,
        'Gungnir_NATIVE_API_CONTRACT "development"' in package,
        "Installed CMake package native API contract must be development",
    )
    require(
        errors,
        'Gungnir_NATIVE_ABI_EPOCH "0"' in package,
        "Installed CMake package ABI epoch must be 0",
    )

    require(
        errors,
        "Project status: **Development**" in readme,
        "README must identify the project as Development",
    )
    require(
        errors,
        "1.0 will be assigned only after the completeness gate is satisfied" in readme,
        "README must explain the 1.0 release rule",
    )

    forbidden_current = (
        "Current release candidate:",
        "Current stable release:",
        "1.0.0-rc.1",
        "Gungnir 1.0 release candidate",
        "feature-frozen for the 1.0",
        "compatibility=stable",
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
        print("Development consistency contract failed:", file=sys.stderr)

        for error in errors:
            print(f" - {error}", file=sys.stderr)

        return 1

    print("Gungnir development consistency contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
