"""Compile the public database transaction example through the strict compiler."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
guide = Path(__file__).resolve().parents[1] / 'docs/database.md'
snippets = re.findall(r'```gnr\s*\n(.*?)```', guide.read_text(), re.S)
assert snippets, 'Missing public database language example'
prelude = """model Project { fillable = ['name']; string name; }
model AuditEntry { fillable = ['project_id', 'action']; int project_id; string action; }
"""
with tempfile.TemporaryDirectory(prefix='gungnir-docs-database-') as directory:
    for index, snippet in enumerate(snippets):
        path = Path(directory) / f'database-{index}.gnr'
        path.write_text(prelude + 'function void example(Json data) {\n' + snippet + '\n}\n')
        result = subprocess.run([str(compiler), str(path), '--check', '--strict'], text=True, capture_output=True)
        assert result.returncode == 0, result.stdout + result.stderr
print(f'Validated all {len(snippets)} public database language examples')
