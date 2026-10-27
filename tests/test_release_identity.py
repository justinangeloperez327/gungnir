"""Exercise release identity resolution against real local Git repositories."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from resolve_release_identity import git, resolve


class ReleaseIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='gungnir release ')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        git(self.root, 'config', 'user.name', 'Release test')
        git(self.root, 'config', 'user.email', 'release-test@example.invalid')
        # Query a real remote, without requiring network access or credentials.
        git(self.root, 'remote', 'add', 'origin', str(self.root))
        (self.root / 'CMakeLists.txt').write_text(
            'project(gungnir VERSION 1.0.1 LANGUAGES CXX)\n'
            'set(GUNGNIR_RELEASE_CHANNEL "stable")\n')
        git(self.root, 'add', 'CMakeLists.txt')
        git(self.root, 'commit', '-qm', 'Initial release source')
        self.source = git(self.root, 'rev-parse', 'HEAD')

    def manual(self, tag='v1.0.1'):
        return resolve(self.root, 'workflow_dispatch', 'refs/heads/main', tag)

    def test_manual_release_can_create_a_missing_tag(self):
        self.assertEqual(self.manual(), {'enabled': 'true', 'tag': 'v1.0.1', 'source': self.source})
        self.assertEqual(git(self.root, 'tag', '--list'), '')

    def test_manual_version_mismatch_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'differs from selected source'):
            self.manual('v1.0.0')

    def test_tag_push_must_match_source_package_version(self):
        path = self.root / 'CMakeLists.txt'
        path.write_text(path.read_text().replace('1.0.1', '1.0.2'))
        git(self.root, 'add', 'CMakeLists.txt')
        git(self.root, 'commit', '-qm', 'Source still declares 1.0.2')
        git(self.root, 'tag', 'v1.0.3')
        with self.assertRaisesRegex(ValueError, 'before tagging it'):
            resolve(self.root, 'push', 'refs/tags/v1.0.3', 'v1.0.3')

    def test_corrected_103_source_and_matching_tag_are_accepted(self):
        path = self.root / 'CMakeLists.txt'
        path.write_text(path.read_text().replace('1.0.1', '1.0.3'))
        git(self.root, 'add', 'CMakeLists.txt')
        git(self.root, 'commit', '-qm', 'Prepare 1.0.3')
        git(self.root, 'tag', 'v1.0.3')
        identity = resolve(self.root, 'push', 'refs/tags/v1.0.3', 'v1.0.3')
        self.assertEqual(identity['tag'], 'v1.0.3')
        self.assertEqual(identity['source'], git(self.root, 'rev-parse', 'HEAD'))

    def test_existing_matching_annotated_tag_is_accepted(self):
        git(self.root, 'tag', '-a', 'v1.0.1', '-m', 'Release')
        self.assertEqual(self.manual()['source'], self.source)

    def test_conflicting_tag_is_rejected_without_moving_it(self):
        git(self.root, 'tag', 'v1.0.1')
        git(self.root, 'commit', '--allow-empty', '-qm', 'Later source')
        with self.assertRaisesRegex(ValueError, 'published tags will not be moved'):
            self.manual()
        self.assertEqual(git(self.root, 'rev-parse', 'v1.0.1'), self.source)

    def test_main_push_does_not_republish_existing_version(self):
        git(self.root, 'tag', 'v1.0.1')
        git(self.root, 'commit', '--allow-empty', '-qm', 'Later source')
        self.assertEqual(resolve(self.root, 'push', 'refs/heads/main', ''), {'enabled': 'false'})

    def test_main_push_can_publish_a_new_version(self):
        self.assertEqual(resolve(self.root, 'push', 'refs/heads/main', '')['source'], self.source)

    def test_pull_request_validation_does_not_require_a_new_tag(self):
        git(self.root, 'tag', 'v1.0.1')
        git(self.root, 'commit', '--allow-empty', '-qm', 'Proposed change')
        identity = resolve(self.root, 'pull_request', 'refs/pull/1/merge', '')
        self.assertEqual(identity['enabled'], 'true')
        self.assertEqual(identity['source'], git(self.root, 'rev-parse', 'HEAD'))

    def test_non_stable_source_is_disabled(self):
        path = self.root / 'CMakeLists.txt'
        path.write_text(path.read_text().replace('"stable"', '"development"'))
        self.assertEqual(self.manual(), {'enabled': 'false'})

    def test_entry_point_writes_the_pinned_outputs(self):
        output = self.root / 'github-output'
        script = Path(__file__).resolve().parents[1] / 'tools/resolve_release_identity.py'
        result = subprocess.run([sys.executable, str(script)], cwd=self.root, text=True,
                                capture_output=True, env=dict(os.environ,
                                GITHUB_EVENT_NAME='workflow_dispatch', GITHUB_REF='refs/heads/main',
                                REQUESTED_TAG='v1.0.1', GITHUB_OUTPUT=str(output)))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(f'source={self.source}\n', output.read_text())
        self.assertIn('tag=v1.0.1\n', output.read_text())


if __name__ == '__main__':
    unittest.main()
