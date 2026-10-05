"""Validate every .gnr snippet in the application-service public guides."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = Path(sys.argv[1]).resolve()
prelude = """model User {string name; string password;}
model Project {int owner_id; string title;}
controller AccountController {show() {return text('account');}}
"""
count = 0
with tempfile.TemporaryDirectory(prefix="gungnir-docs-application-") as temporary:
    for name in ("view", "policy", "authentication", "dependency-injection", "configuration", "logging-observability"):
        text = (root / f"docs/{name}.md").read_text()
        snippets = re.findall(r"```gnr\s*\n(.*?)```", text, re.S)
        assert snippets, f"No language examples in docs/{name}.md"
        for number, snippet in enumerate(snippets, 1):
            if re.match(r"\s*(controller|function|policy|Route::)\b", snippet):
                source = prelude + snippet
            else:
                result_type = "Response" if snippet.lstrip().startswith("return ") else "void"
                source = prelude + f"function {result_type} example(Request request, List<User> users) {{\n" + snippet + "\n}\n"
            path = Path(temporary) / f"{name}-{number}.gnr"
            path.write_text(source)
            result = subprocess.run([str(compiler), str(path), "--check", "--strict"], text=True, capture_output=True)
            assert result.returncode == 0, f"docs/{name}.md snippet {number}\n{result.stdout}{result.stderr}"
            count += 1
print(f"Validated all {count} public application-service language examples")
