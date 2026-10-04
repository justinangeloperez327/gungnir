"""Strictly validate every public validation example in its action context."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = Path(sys.argv[1]).resolve()
snippets = re.findall(r"```gnr\s*\n(.*?)```", (root / "docs/validation.md").read_text(), re.S)
assert snippets, "No public validation examples"
with tempfile.TemporaryDirectory(prefix="gungnir-docs-validation-") as temporary:
    for number, snippet in enumerate(snippets, 1):
        source = snippet if snippet.lstrip().startswith(("function ", "controller ")) else "function void example(Request request) {\n" + snippet + "\n}"
        path = Path(temporary) / f"example-{number}.gnr"
        path.write_text(source)
        result = subprocess.run([str(compiler), str(path), "--check", "--strict"], text=True, capture_output=True)
        if result.returncode:
            raise AssertionError(f"docs/validation.md snippet {number}\n{result.stdout}{result.stderr}")
print(f"Validated {len(snippets)} public validation examples")
