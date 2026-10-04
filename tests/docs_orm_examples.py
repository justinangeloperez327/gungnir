"""Check the public model/ORM snippets in their documented application context."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = Path(sys.argv[1]).resolve()
models = (root / "tests/fixtures/structured/orm.gnr").read_text().split("migration CreateOrmTables")[0]
companions = ("User", "Post", "Profile", "Role", "Comment", "Project", "Tenant")
count = 0
with tempfile.TemporaryDirectory(prefix="gungnir-docs-orm-") as temporary:
    for name in ("model", "orm", "relationships", "collection"):
        text = (root / f"docs/{name}.md").read_text()
        for number, snippet in enumerate(re.findall(r"```gnr\s*\n(.*?)```", text, re.S), 1):
            declared = set(re.findall(r"\bmodel\s+(\w+)", snippet))
            if declared:
                source = snippet + "\n" + "\n".join(f"model {model} {{}}" for model in companions if model not in declared)
            else:
                # Guide fragments execute inside a controller/function with
                # request identifiers and an already retrieved model.
                setup = "" if re.search(r"\b(?:let|const)\s+user\b", snippet) else "let user = User::findOrFail(id);\n"
                source = models + "\nfunction void example(int id, int role_id) {\n" + setup + snippet + "\n}\n"
            path = Path(temporary) / f"{name}-{number}.gnr"
            path.write_text(source)
            result = subprocess.run([str(compiler), str(path), "--check", "--strict"], text=True, capture_output=True)
            if result.returncode:
                raise AssertionError(f"docs/{name}.md snippet {number}\n{result.stdout}{result.stderr}")
            count += 1
print(f"Validated {count} public model/ORM/relationship/collection examples")
