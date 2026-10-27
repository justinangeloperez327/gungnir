"""Test the actual publication step with offline Git/GitHub command stubs."""
from pathlib import Path
import os
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[1]

class ReleasePublicationTests(unittest.TestCase):
    def publish(self, exists, count="0", source="abc123"):
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        step = workflow.split("- name: Publish release assets", 1)[1]
        script = textwrap.dedent(step.split("run: |\n", 1)[1])
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "release-assets").mkdir()
            (root / "release-assets/setup.exe").write_text("fixture")
            (root / "git").write_text("#!/usr/bin/env python3\nimport os,sys\nif '--quiet' not in sys.argv: print(os.environ['TAG_SOURCE'])\n")
            (root / "gh").write_text(r"""#!/usr/bin/env python3
import os,sys
with open(os.environ['CALLS'], 'a') as log: log.write(' '.join(sys.argv[1:]) + '\n')
if sys.argv[1:3] == ['release','view']:
    if os.environ['EXISTS'] == '0': sys.exit(1)
    if '--json' in sys.argv: print(os.environ['ASSET_COUNT'])
""")
            for name in ("git", "gh"): (root / name).chmod(0o755)
            calls = root / "calls"
            result = subprocess.run(["bash", "-c", script], cwd=root,
                text=True, capture_output=True, env=dict(os.environ,
                PATH=str(root) + os.pathsep + os.environ["PATH"],
                RELEASE_TAG="v1.0.3", RELEASE_SOURCE="abc123",
                GITHUB_REPOSITORY="example/gungnir", TAG_SOURCE=source,
                EXISTS="1" if exists else "0", ASSET_COUNT=count, CALLS=str(calls)))
            return result, calls.read_text() if calls.exists() else ""

    def test_existing_empty_release_gets_assets(self):
        result, calls = self.publish(True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("release upload v1.0.3", calls)
        self.assertNotIn("--clobber", calls)
        self.assertNotIn("release create", calls)

    def test_existing_assets_are_never_overwritten(self):
        result, calls = self.publish(True, count="2")
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("release upload", calls)
        self.assertNotIn("release create", calls)

    def test_missing_release_is_created(self):
        result, calls = self.publish(False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("release create v1.0.3", calls)
        self.assertIn("--verify-tag", calls)

    def test_conflicting_source_stops_before_publication(self):
        result, calls = self.publish(True, source="different")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(calls, "")

if __name__ == "__main__":
    unittest.main()
