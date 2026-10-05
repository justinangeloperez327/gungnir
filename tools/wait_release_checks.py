#!/usr/bin/env python3
"""Require successful CI on the exact commit before a release is published."""
import json
import os
import sys
import time
import urllib.request

required = {"CI", "Compiler Conformance", "Native API ABI", "Documentation", "Release Consistency", "Performance Baseline"}
repository, commit = sys.argv[1:3]
deadline = time.monotonic() + 25 * 60
while True:
    request = urllib.request.Request(
        f"https://api.github.com/repos/{repository}/actions/runs?head_sha={commit}&per_page=100",
        headers={"Authorization": "Bearer " + os.environ["GH_TOKEN"], "Accept": "application/vnd.github+json"})
    with urllib.request.urlopen(request, timeout=30) as response:
        runs = json.load(response)["workflow_runs"]
    # Latest run per workflow wins. Never reuse a successful earlier attempt.
    latest = {}
    for run in sorted(runs, key=lambda item: item["id"], reverse=True):
        if run["name"] in required and run["head_sha"] == commit:
            latest.setdefault(run["name"], run)
    failed = [name for name, run in latest.items() if run["status"] == "completed" and run["conclusion"] != "success"]
    if failed:
        raise SystemExit("Release blocked by failed exact-commit checks: " + ", ".join(sorted(failed)))
    pending = sorted(required - {name for name, run in latest.items() if run["status"] == "completed" and run["conclusion"] == "success"})
    if not pending:
        print("All required workflows passed on " + commit)
        break
    if time.monotonic() >= deadline:
        raise SystemExit("Release checks did not complete: " + ", ".join(pending))
    print("Waiting for: " + ", ".join(pending), flush=True)
    time.sleep(30)
