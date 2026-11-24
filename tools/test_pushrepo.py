#!/usr/bin/env python3
"""Test publication against a local bare repository; no network or hardware."""
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
import pushrepo
from branch_checkout import git


class PushRepo(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fx9a-push-')
        self.addCleanup(self.temp.cleanup)
        parent = Path(self.temp.name)
        self.root = parent/'source'
        self.root.mkdir()
        self.remote = parent/'remote.git'
        self.remote.mkdir()
        git(self.remote, 'init', '--bare')
        git(self.root, 'init', '-b', 'master')
        git(self.root, 'config', 'user.name', 'Test')
        git(self.root, 'config', 'user.email', 'test@example.invalid')
        (self.root/'file').write_text('initial\n')
        git(self.root, 'add', 'file')
        git(self.root, 'commit', '-m', 'initial')
        git(self.root, 'remote', 'add', 'origin', str(self.remote))
        self.patch = patch.object(pushrepo, 'ROOT', self.root)
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def cli(self, *args):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return pushrepo.main(list(args))

    def test_push_and_verify_selected_branch(self):
        git(self.root, 'branch', 'cmsis-ll-port')
        self.assertEqual(self.cli('--branch', 'cmsis-ll-port'), 0)
        self.assertEqual(git(self.root, 'branch', '--show-current').stdout.strip(), 'cmsis-ll-port')
        local = git(self.root, 'rev-parse', 'HEAD').stdout.strip()
        self.assertEqual(git(self.remote, 'rev-parse', 'refs/heads/cmsis-ll-port').stdout.strip(), local)
        self.assertFalse(git(self.root, 'status', '--porcelain').stdout)

    def test_dirty_worktree_refused_without_switch_or_push(self):
        git(self.root, 'branch', 'other')
        (self.root/'file').write_text('keep edits\n')
        self.assertEqual(self.cli('--branch', 'other'), 1)
        self.assertEqual(git(self.root, 'branch', '--show-current').stdout.strip(), 'master')
        self.assertEqual((self.root/'file').read_text(), 'keep edits\n')
        self.assertFalse(git(self.remote, 'for-each-ref').stdout)

    def test_dry_run_never_switches_or_pushes(self):
        git(self.root, 'branch', 'other')
        self.assertEqual(self.cli('--branch', 'other', '--dry-run'), 0)
        self.assertEqual(git(self.root, 'branch', '--show-current').stdout.strip(), 'master')
        self.assertFalse(git(self.remote, 'for-each-ref').stdout)

    def test_remote_ahead_is_not_force_pushed(self):
        initial = git(self.root, 'rev-parse', 'HEAD').stdout.strip()
        (self.root/'file').write_text('remote ahead\n')
        git(self.root, 'commit', '-am', 'ahead')
        git(self.root, 'push', 'origin', 'master')
        ahead = git(self.root, 'rev-parse', 'HEAD').stdout.strip()
        git(self.root, 'branch', 'old', initial)
        git(self.remote, 'update-ref', 'refs/heads/old', ahead)
        self.assertEqual(self.cli('--branch', 'old'), 1)
        self.assertEqual(git(self.remote, 'rev-parse', 'refs/heads/old').stdout.strip(), ahead)


if __name__ == '__main__':
    unittest.main()
