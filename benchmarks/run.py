#!/usr/bin/env python3

import argparse
import json
import os
import platform
import subprocess
from pathlib import Path


BENCHMARKS = (
    ("compiler", "gungnir_benchmark_compiler"),
    ("http-routing", "gungnir_benchmark_http_routing"),
    ("orm-query-compiler", "gungnir_benchmark_orm"),
)


def executable(build_dir: Path, name: str) -> Path:
    candidates = [
        build_dir / name,
        build_dir / f"{name}.exe",
        build_dir / "Release" / f"{name}.exe",
        build_dir / "Release" / name,
    ]

    for candidate in candidates:
        if candidate.exists():
            return candidate

    raise FileNotFoundError(
        f"Unable to find benchmark executable {name} in {build_dir}"
    )


def run_suite(path: Path, smoke: bool) -> dict:
    command = [str(path), "--json"]

    if smoke:
        command.append("--smoke")

    completed = subprocess.run(
        command,
        check=True,
        capture_output=True,
        text=True,
    )

    payload = completed.stdout.strip().splitlines()

    if not payload:
        raise RuntimeError(f"{path.name} produced no benchmark output")

    result = json.loads(payload[-1])

    if result.get("schema") != 1:
        raise RuntimeError(f"{path.name} emitted an unsupported schema")

    return result


def markdown(payload: dict) -> str:
    lines = [
        "# Gungnir benchmark results",
        "",
        f"- OS: {payload['environment']['platform']}",
        f"- Machine: {payload['environment']['machine']}",
        f"- Commit: {payload['environment']['commit'] or 'local'}",
        f"- Mode: {'smoke' if payload['smoke'] else 'measurement'}",
        "",
        "| Suite | Case | Median ns/op | p95 ns/op | Ops/s |",
        "| --- | --- | ---: | ---: | ---: |",
    ]

    for suite in payload["suites"]:
        for result in suite["results"]:
            lines.append(
                "| "
                f"{suite['suite']} | {result['name']} | "
                f"{result['median_ns_per_operation']:.3f} | "
                f"{result['p95_ns_per_operation']:.3f} | "
                f"{result['operations_per_second']:.3f} |"
            )

    lines.append("")
    lines.append(
        "Wall-clock values are observational. Compare runs only on like-for-like "
        "hardware/toolchains and use correctness CI for deterministic gates."
    )

    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--output", default="benchmark-results.json")
    parser.add_argument("--summary")
    parser.add_argument("--smoke", action="store_true")
    args = parser.parse_args()

    build_dir = Path(args.build_dir).resolve()

    suites = [
        run_suite(executable(build_dir, binary), args.smoke)
        for _, binary in BENCHMARKS
    ]

    payload = {
        "schema": 1,
        "smoke": args.smoke,
        "environment": {
            "platform": platform.platform(),
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "processor": platform.processor(),
            "python": platform.python_version(),
            "commit": os.environ.get("GITHUB_SHA", ""),
            "runner_os": os.environ.get("RUNNER_OS", ""),
        },
        "suites": suites,
    }

    output = Path(args.output)
    output.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    summary_text = markdown(payload)

    if args.summary:
        Path(args.summary).write_text(summary_text, encoding="utf-8")

    print(summary_text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
