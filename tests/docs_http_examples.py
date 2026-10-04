"""Strictly check HTTP API fragments in their documented action context."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = Path(sys.argv[1]).resolve()
models = "model User { string name; }\n"
count = 0
with tempfile.TemporaryDirectory(prefix="gungnir-docs-http-") as temporary:
    for name in ("request", "response", "middleware"):
        text = (root / f"docs/{name}.md").read_text()
        for number, snippet in enumerate(re.findall(r"```gnr\s*\n(.*?)```", text, re.S), 1):
            if name == "middleware":
                # Route registration uses the application's route compiler;
                # this checker covers the canonical middleware declarations.
                if not snippet.lstrip().startswith("middleware "):
                    continue
                source = snippet
            else:
                result_type = "void" if name == "request" else "Response"
                source = models + f"function {result_type} example(Request request, User user, List<User> users) {{\n" + snippet + "\n}\n"
            path = Path(temporary) / f"{name}-{number}.gnr"
            path.write_text(source)
            result = subprocess.run([str(compiler), str(path), "--check", "--strict"], text=True, capture_output=True)
            if result.returncode:
                raise AssertionError(f"docs/{name}.md snippet {number}\n{result.stdout}{result.stderr}")
            count += 1
print(f"Validated {count} public request/response examples and middleware declarations")
