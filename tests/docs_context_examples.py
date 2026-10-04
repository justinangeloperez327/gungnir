"""Check the public session, identity, and cookie fragments in action context."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = Path(sys.argv[1]).resolve()
count = 0
with tempfile.TemporaryDirectory(prefix="gungnir-docs-context-") as temporary:
    for name in ("session", "authentication", "response"):
        text = (root / f"docs/{name}.md").read_text()
        for number, snippet in enumerate(re.findall(r"```gnr\s*\n(.*?)```", text, re.S), 1):
            # Credential-attempt and route-registration fragments have their
            # own application contracts. This gate covers identity snapshots.
            if name == "authentication" and "request.user()" not in snippet:
                continue
            if name == "response" and not re.search(r"\.(?:cookie|withoutCookie)\(", snippet):
                continue
            result_type = "Response" if name == "response" else "void"
            source = f"function {result_type} example(Request request) {{\n" + snippet + "\n}\n"
            path = Path(temporary) / f"{name}-{number}.gnr"
            path.write_text(source)
            result = subprocess.run([str(compiler), str(path), "--check", "--strict"], text=True, capture_output=True)
            if result.returncode:
                raise AssertionError(f"docs/{name}.md snippet {number}\n{result.stdout}{result.stderr}")
            count += 1
print(f"Validated {count} public session, identity, and cookie examples")
