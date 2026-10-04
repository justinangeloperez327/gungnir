"""Strictly validate the public login/logout declarations and password fragment."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = Path(sys.argv[1]).resolve()
count = 0
with tempfile.TemporaryDirectory(prefix="gungnir-docs-auth-") as temporary:
    for number, snippet in enumerate(re.findall(r"```gnr\s*\n(.*?)```", (root / "docs/authentication.md").read_text(), re.S), 1):
        # Identity snapshots and application route assembly have independent
        # contracts. This gate checks the credential and password APIs.
        if not re.search(r"\bauth\.(?:attempt|logout)\(|\bPassword::", snippet):
            continue
        source = snippet if snippet.lstrip().startswith("controller ") else "function void example(Request request) {\n" + snippet + "\n}\n"
        path = Path(temporary) / f"auth-{number}.gnr"
        path.write_text(source)
        result = subprocess.run([str(compiler), str(path), "--check", "--strict"], text=True, capture_output=True)
        if result.returncode:
            raise AssertionError(f"docs/authentication.md snippet {number}\n{result.stdout}{result.stderr}")
        count += 1
assert count == 3, f"Expected three credential/password examples, got {count}"
print(f"Validated {count} public authentication and password examples")
