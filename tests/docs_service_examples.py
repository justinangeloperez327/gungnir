"""Validate every cache/storage language example through the canonical compiler."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = Path(sys.argv[1]).resolve()
count = 0
with tempfile.TemporaryDirectory(prefix="gungnir-docs-services-") as temporary:
    for guide in ("cache.md", "storage.md"):
        for number, source in enumerate(re.findall(r"```gnr\s*\n(.*?)```", (root / "docs" / guide).read_text(), re.S), 1):
            path = Path(temporary) / f"{guide}-{number}.gnr"
            path.write_text(source)
            result = subprocess.run([str(compiler), str(path), "--check", "--strict"], text=True, capture_output=True)
            if result.returncode:
                raise AssertionError(f"docs/{guide} snippet {number}\n{result.stdout}{result.stderr}")
            count += 1
assert count == 7, f"Expected seven cache/storage examples, got {count}"
print(f"Validated {count} public cache/storage examples")
