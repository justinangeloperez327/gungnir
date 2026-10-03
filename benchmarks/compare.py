#!/usr/bin/env python3

import argparse
import json
from pathlib import Path


def load(path: str) -> dict:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def index(payload: dict) -> dict[tuple[str, str], dict]:
    indexed = {}

    for suite in payload["suites"]:
        suite_name = suite["suite"]

        for result in suite["results"]:
            indexed[(suite_name, result["name"])] = result

    return indexed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("baseline")
    parser.add_argument("candidate")
    parser.add_argument(
        "--fail-above",
        type=float,
        help=(
            "Fail when candidate median exceeds baseline by this ratio. "
            "Example: 1.25 means a 25%% regression budget."
        ),
    )
    args = parser.parse_args()

    baseline = index(load(args.baseline))
    candidate = index(load(args.candidate))

    keys = sorted(set(baseline) | set(candidate))
    failed = False

    print("| Suite | Case | Baseline ns/op | Candidate ns/op | Ratio |")
    print("| --- | --- | ---: | ---: | ---: |")

    for key in keys:
        before = baseline.get(key)
        after = candidate.get(key)

        if before is None or after is None:
            print(
                f"| {key[0]} | {key[1]} | "
                f"{'-' if before is None else before['median_ns_per_operation']} | "
                f"{'-' if after is None else after['median_ns_per_operation']} | n/a |"
            )
            continue

        baseline_ns = float(before["median_ns_per_operation"])
        candidate_ns = float(after["median_ns_per_operation"])

        ratio = (
            candidate_ns / baseline_ns
            if baseline_ns > 0.0
            else float("inf")
        )

        print(
            f"| {key[0]} | {key[1]} | "
            f"{baseline_ns:.3f} | {candidate_ns:.3f} | {ratio:.3f}x |"
        )

        if (
            args.fail_above is not None
            and ratio > args.fail_above
        ):
            failed = True

    if args.fail_above is None:
        print(
            "\nInformational comparison only. "
            "No wall-clock failure threshold was requested."
        )
    elif failed:
        print(
            f"\nAt least one benchmark exceeded the "
            f"{args.fail_above:.3f}x regression budget."
        )
    else:
        print(
            f"\nAll comparable medians stayed within the "
            f"{args.fail_above:.3f}x regression budget."
        )

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
