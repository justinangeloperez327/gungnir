#!/usr/bin/env python3
"""Pin packaging to the selected source and reject conflicting release tags."""
from pathlib import Path
import os
import re
import subprocess
import sys


def git(root, *arguments):
    return subprocess.run(['git', '-C', str(root), *arguments], text=True,
                          capture_output=True, timeout=30, check=True).stdout.strip()


def resolve(root, event, ref, requested_tag):
    cmake = (root / 'CMakeLists.txt').read_text()
    channel = re.search(r'^set\(GUNGNIR_RELEASE_CHANNEL "([^"]+)"\)', cmake, re.M)
    if not channel or channel[1] != 'stable':
        return {'enabled': 'false'}
    version = re.search(r'^project\(gungnir VERSION (\d+\.\d+\.\d+) LANGUAGES CXX\)', cmake, re.M)
    if not version:
        raise ValueError('Unable to read the stable package version from CMakeLists.txt')
    tag = 'v' + version[1]
    if requested_tag and requested_tag != tag:
        raise ValueError(f'Requested tag {requested_tag} differs from selected source version {tag}. '
                         'Update and validate the source package version before tagging it; '
                         'rerunning this tag still checks out the same mismatched source.')
    source = git(root, 'rev-parse', 'HEAD')
    if event != 'pull_request':
        tag_ref = 'refs/tags/' + tag
        result = subprocess.run(['git', '-C', str(root), 'ls-remote', '--exit-code',
                                 '--tags', 'origin', tag_ref, tag_ref + '^{}'],
                                text=True, capture_output=True, timeout=30)
        if result.returncode not in (0, 2):
            raise ValueError('Unable to check the remote release tag: ' + result.stderr.strip())
        remote = dict(line.split()[::-1] for line in result.stdout.splitlines())
        existing = remote.get(tag_ref + '^{}', remote.get(tag_ref))
        if existing and event == 'push' and ref == 'refs/heads/main':
            # Ordinary development after a release must not republish it.
            return {'enabled': 'false'}
        if existing and existing != source:
            raise ValueError(f'Tag {tag} points to {existing}, but selected source is {source}. '
                             'Select the matching source or use a new release version; '
                             'published tags will not be moved automatically.')
    return {'enabled': 'true', 'tag': tag, 'source': source}


def main():
    try:
        identity = resolve(Path.cwd(), os.environ['GITHUB_EVENT_NAME'],
                           os.environ['GITHUB_REF'], os.environ.get('REQUESTED_TAG', ''))
    except (ValueError, subprocess.SubprocessError) as error:
        print(f'::error::{error}', file=sys.stderr)
        return 1
    output = ''.join(f'{name}={value}\n' for name, value in identity.items())
    with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as target:
        target.write(output)
    print(output, end='')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
