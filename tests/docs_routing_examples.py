"""Validate each public routing fragment in a complete canonical project."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = Path(sys.argv[1]).resolve()
stubs = {
    "app/models/Project.gnr": 'model Project { table = "projects"; string name; }',
    "app/controllers/HomeController.gnr": 'controller HomeController { index() { return text("home"); } }',
    "app/controllers/UserController.gnr": """controller UserController {
        store() { return noContent(); }
        update(int user) { return json(user); }
        destroy(int user) { return noContent(); }
        options() { return noContent(); }
        show(int id) { return json(id); }
        showCode(string code) { return text(code); }
    }""",
    "app/controllers/DashboardController.gnr": 'controller DashboardController { index() { return noContent(); } }',
    "app/controllers/AccountController.gnr": 'controller AccountController { show() { return noContent(); } }',
    "app/controllers/AdminUserController.gnr": 'controller AdminUserController { index() { return noContent(); } }',
    "app/controllers/ReportController.gnr": 'controller ReportController { index() { return noContent(); } }',
    "app/middleware/AuditMiddleware.gnr": 'middleware AuditMiddleware { async Response handle(Request request, Next next) { return await next(request); } }',
}
snippets = re.findall(r"```gnr\s*\n(.*?)```", (root / "docs/routing.md").read_text(), re.S)
assert len(snippets) == 10, f"Expected ten public routing examples, got {len(snippets)}"
with tempfile.TemporaryDirectory(prefix="gungnir-docs-routing-") as temporary:
    for number, source in enumerate(snippets, 1):
        project = Path(temporary) / str(number)
        for name, content in {**stubs, "routes/guide.gnr": source}.items():
            path = project / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
        result = subprocess.run([str(compiler), str(project), "--project", "--check", "--strict"], text=True, capture_output=True)
        if result.returncode:
            raise AssertionError(f"docs/routing.md snippet {number}\n{result.stdout}{result.stderr}")
print(f"Validated {len(snippets)} public routing examples")
