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
RELEASE_CHANNEL_PATTERN = re.compile(
    r'set\(GUNGNIR_RELEASE_CHANNEL "([^"]*)"\)'
)


def fail(errors: list[str], message: str) -> None:
    errors.append(message)


def project_identity(errors: list[str]) -> tuple[str, str]:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = PROJECT_VERSION_PATTERN.search(cmake)

    if not match:
        fail(errors, "Unable to determine internal CMake version")
        return "unknown", "unknown"

    channel_match = RELEASE_CHANNEL_PATTERN.search(cmake)

    if not channel_match:
        fail(errors, "Unable to determine GUNGNIR_RELEASE_CHANNEL")
        return match.group(1), "unknown"

    return match.group(1), channel_match.group(1)


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

    result.extend(sorted((ROOT / "docs").rglob("*.md")))
    result.extend(sorted((ROOT / "examples").rglob("*.md")))

    return [path for path in result if path.is_file()]


def normalized_link_target(raw: str) -> str:
    value = raw.strip()

    if value.startswith("<") and value.endswith(">"):
        value = value[1:-1]

    value = value.split("#", 1)[0]
    value = value.split("?", 1)[0]
    return unquote(value)


def prose_without_fenced_code(text: str) -> str:
    output: list[str] = []
    fence: str | None = None

    for line in text.splitlines():
        stripped = line.lstrip()

        if fence is None:
            if stripped.startswith("```"):
                fence = "```"
                continue

            if stripped.startswith("~~~"):
                fence = "~~~"
                continue

            output.append(line)
            continue

        if stripped.startswith(fence):
            fence = None

    return "\n".join(output)


def check_relative_links(errors: list[str]) -> None:
    for source in markdown_files():
        text = prose_without_fenced_code(
            source.read_text(encoding="utf-8")
        )

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

    design_index = (
        ROOT / "docs" / "design" / "README.md"
    ).read_text(encoding="utf-8")

    for path in sorted((ROOT / "docs" / "design").glob("*.md")):
        if path.name == "README.md":
            continue

        if f"({path.name})" not in design_index:
            fail(
                errors,
                f"docs/design/README.md does not index {path.name}",
            )


def check_identity_contract(
    errors: list[str],
    internal_version: str,
    release_channel: str,
) -> None:
    readme = (ROOT / "README.md").read_text(encoding="utf-8")

    if internal_version != "0.0.0":
        fail(
            errors,
            f"Development CMake placeholder must remain 0.0.0, got {internal_version}",
        )

    if release_channel != "development":
        fail(
            errors,
            f"Development release channel must be development, got {release_channel}",
        )

    expected_status = "Project status: **Development**"

    if expected_status not in readme:
        fail(errors, f"README.md must contain: {expected_status}")

    stale = (
        "Current release candidate:",
        "Current stable release:",
        "1.0.0-rc.1",
        "Gungnir 1.0 release candidate",
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
                    f"{source.relative_to(ROOT)} contains stale maturity reference {value}",
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
    internal_version, release_channel = project_identity(errors)
    check_relative_links(errors)
    check_docs_index(errors)
    check_identity_contract(
        errors,
        internal_version,
        release_channel,
    )
    check_example_contract(errors)

    if errors:
        print("Documentation contract failed:", file=sys.stderr)

        for error in errors:
            print(f" - {error}", file=sys.stderr)

        return 1

    print("Documentation contract passed for Gungnir development.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
