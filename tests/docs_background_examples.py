"""Strictly validate every public event/listener/queue/scheduler example."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
root = Path(__file__).resolve().parents[1]
compiler = Path(sys.argv[1]).resolve()
prelude = "event UserRegistered { int user_id; string email; }\njob GenerateReport { int report_id; handle() {} }\n"
count = 0
with tempfile.TemporaryDirectory(prefix="gungnir-docs-background-") as temporary:
    for guide in ("event", "listener", "queues", "scheduler"):
        snippets = re.findall(r"```gnr\s*\n(.*?)```", (root / f"docs/{guide}.md").read_text(), re.S)
        assert snippets, f"No examples in {guide}"
        for number, snippet in enumerate(snippets, 1):
            declarations = prelude
            if "event UserRegistered" in snippet:
                declarations = declarations.replace("event UserRegistered { int user_id; string email; }\n", "")
            if "job GenerateReport" in snippet:
                declarations = declarations.replace("job GenerateReport { int report_id; handle() {} }\n", "")
            path = Path(temporary) / f"{guide}-{number}.gnr"
            path.write_text(declarations + snippet)
            result = subprocess.run([str(compiler), str(path), "--check", "--strict"], text=True, capture_output=True)
            if result.returncode:
                raise AssertionError(f"docs/{guide}.md example {number}\n{result.stdout}{result.stderr}")
            count += 1
print(f"Validated {count} public background examples")
