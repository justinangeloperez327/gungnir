#!/usr/bin/env python3
"""Validate Gungnir's public documentation contract."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from urllib.parse import unquote


ROOT = Path(__file__).resolve().parents[1]

REQUIRED_PROJECT_FILES = (
    "README.md",
    "CHANGELOG.md",
    "CONTRIBUTING.md",
    "SECURITY.md",
    "SUPPORT.md",
    "LICENSE",
)

PUBLIC_DOC_ROOTS = (
    ROOT / "README.md",
    ROOT / "docs" / "getting-started.md",
    ROOT / "docs" / "stability.md",
)

LINK_PATTERN = re.compile(r"(?<!!)\[[^\]]+\]\(([^)]+)\)")
PROJECT_VERSION_PATTERN = re.compile(
    r"project\(gungnir VERSION (\d+\.\d+\.\d+) LANGUAGES CXX\)"
)
PRERELEASE_PATTERN = re.compile(
    r'set\(GUNGNIR_VERSION_PRERELEASE "([^"]*)"\)'
)


def fail(errors: list[str], message: str) -> None:
    errors.append(message)


def release_version(errors: list[str]) -> str:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = PROJECT_VERSION_PATTERN.search(cmake)

    if not match:
        fail(errors, "Unable to determine project version from CMakeLists.txt")
        return "unknown"

    core = match.group(1)
    prerelease_match = PRERELEASE_PATTERN.search(cmake)
    prerelease = prerelease_match.group(1) if prerelease_match else ""

    return core + (f"-{prerelease}" if prerelease else "")


def check_required_files(errors: list[str]) -> None:
    for relative in REQUIRED_PROJECT_FILES:
        if not (ROOT / relative).is_file():
            fail(errors, f"Missing ecosystem file: {relative}")


def markdown_files() -> list[Path]:
    result = [
        ROOT / "README.md",
        ROOT / "CHANGELOG.md",
        ROOT / "CONTRIBUTING.md",
        ROOT / "SECURITY.md",
        ROOT / "SUPPORT.md",
    ]

    result.extend(sorted((ROOT / "docs").glob("*.md")))
    result.extend(sorted((ROOT / "examples").rglob("*.md")))

    return [path for path in result if path.is_file()]


def normalized_link_target(raw: str) -> str:
    value = raw.strip()

    if value.startswith("<") and value.endswith(">"):
        value = value[1:-1]

    value = value.split("#", 1)[0]
    value = value.split("?", 1)[0]
    return unquote(value)


def check_relative_links(errors: list[str]) -> None:
    for source in markdown_files():
        text = source.read_text(encoding="utf-8")

        for raw in LINK_PATTERN.findall(text):
            target = normalized_link_target(raw)

            if not target:
                continue

            if re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", target):
                continue

            if target.startswith("//"):
                continue

            candidate = (source.parent / target).resolve()

            try:
                candidate.relative_to(ROOT.resolve())
            except ValueError:
                fail(
                    errors,
                    f"{source.relative_to(ROOT)} links outside repository: {raw}",
                )
                continue

            if not candidate.exists():
                fail(
                    errors,
                    f"{source.relative_to(ROOT)} has broken link: {raw}",
                )


def check_docs_index(errors: list[str]) -> None:
    index = (ROOT / "docs" / "README.md").read_text(encoding="utf-8")

    for path in sorted((ROOT / "docs").glob("*.md")):
        if path.name == "README.md":
            continue

        if f"({path.name})" not in index:
            fail(errors, f"docs/README.md does not index {path.name}")


def check_version_contract(errors: list[str], version: str) -> None:
    readme = (ROOT / "README.md").read_text(encoding="utf-8")

    if "-rc." in version:
        expected_status = f"Current release candidate: **v{version}**"
    else:
        expected_status = f"Current stable release: **v{version}**"

    if expected_status not in readme:
        fail(errors, f"README.md must contain: {expected_status}")

    expected_linux = f"gungnir-v{version}-linux-x86_64.tar.gz"

    if expected_linux not in readme:
        fail(errors, f"README.md must show current Linux asset {expected_linux}")

    stale = (
        "Current public preview: **v0.9.0**",
        "Gungnir 0.9 preview",
        "v0.1.0",
        "v0.1.1",
        "v0.1.2",
    )

    for source in PUBLIC_DOC_ROOTS:
        text = source.read_text(encoding="utf-8")

        for value in stale:
            if value in text:
                fail(
                    errors,
                    f"{source.relative_to(ROOT)} contains stale status reference {value}",
                )


def check_example_contract(errors: list[str]) -> None:
    required = (
        ROOT / "examples" / "hello" / "app" / "controllers" / "HomeController.gnr",
        ROOT / "examples" / "hello" / "routes" / "web.gnr",
        ROOT / "examples" / "hello" / "README.md",
    )

    for path in required:
        if not path.is_file():
            fail(
                errors,
                f"Missing canonical hello example file: {path.relative_to(ROOT)}",
            )


def main() -> int:
    errors: list[str] = []

    check_required_files(errors)
    version = release_version(errors)
    check_relative_links(errors)
    check_docs_index(errors)
    check_version_contract(errors, version)
    check_example_contract(errors)

    if errors:
        print("Documentation contract failed:", file=sys.stderr)

        for error in errors:
            print(f" - {error}", file=sys.stderr)

        return 1

    print(f"Documentation contract passed for Gungnir {version}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
