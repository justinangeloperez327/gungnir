"""Strictly validate public mail and notification language examples."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = Path(sys.argv[1]).resolve()
user = 'model User { fillable = ["name", "email"]; casts = {name: "string", email: "string"}; }\n'
mail = 'mail WelcomeMail { string name; subject() { return "Welcome " + name; } }\n'
notification = 'notification WelcomeNotification { via(User user) { return ["mail", "database"]; } toMail(User user) { return WelcomeMail(user.name); } toDatabase(User user) { return {message: "Welcome"}; } }\n'
count = 0
with tempfile.TemporaryDirectory(prefix="gungnir-docs-delivery-") as directory:
    for guide in ("mail", "notification"):
        snippets = re.findall(r"```gnr\s*\n(.*?)```", (root / f"docs/{guide}.md").read_text(), re.S)
        assert snippets, f"No examples in {guide}"
        for number, snippet in enumerate(snippets, 1):
            source = user + ("" if "mail WelcomeMail" in snippet else mail)
            if "notification WelcomeNotification" not in snippet:
                source += notification
            path = Path(directory) / f"{guide}-{number}.gnr"
            path.write_text(source + snippet)
            result = subprocess.run([str(compiler), str(path), "--strict", "--check"], text=True, capture_output=True)
            assert result.returncode == 0, f"docs/{guide}.md example {number}\n{result.stdout}{result.stderr}"
            count += 1
print(f"Validated {count} public delivery examples")
