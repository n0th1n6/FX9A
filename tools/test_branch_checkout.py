#!/usr/bin/env python3
"""Exercise branch switching with real temporary Git repositories; no probe/network."""
from pathlib import Path
import contextlib
import io
import subprocess
import sys
import tempfile
import unittest
sys.dont_write_bytecode = True
from branch_checkout import git, select_branch


class BranchCheckout(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fx9a-branch-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        git(self.root, 'init', '-b', 'master')
        git(self.root, 'config', 'user.name', 'Test')
        git(self.root, 'config', 'user.email', 'test@example.invalid')
        (self.root/'firmware.c').write_text('master\n')
        git(self.root, 'add', '.')
        git(self.root, 'commit', '-m', 'master')
        git(self.root, 'switch', '-c', 'cmsis-ll-port')
        (self.root/'cmsis_ll').mkdir()
        (self.root/'cmsis_ll/CMakeLists.txt').touch()
        (self.root/'firmware.c').write_text('cmsis\n')
        git(self.root, 'add', '.')
        git(self.root, 'commit', '-m', 'cmsis')

    def select(self, branch, dry=False):
        with contextlib.redirect_stdout(io.StringIO()):
            return select_branch(self.root, branch, dry)

    def test_current_branch_uses_dirty_worktree(self):
        (self.root/'firmware.c').write_text('local timing fix\n')
        source, commit, native = self.select('cmsis-ll-port')
        self.assertEqual(source, self.root)
        self.assertTrue(native)
        self.assertEqual((source/'firmware.c').read_text(), 'local timing fix\n')
        self.assertTrue(git(self.root, 'status', '--porcelain').stdout)
        self.assertEqual(len(git(self.root, 'worktree', 'list').stdout.splitlines()), 1)

    def test_switch_and_build_source_is_same_worktree(self):
        source, commit, native = self.select('master')
        self.assertEqual(source, self.root)
        self.assertFalse(native)
        self.assertEqual(git(source, 'branch', '--show-current').stdout.strip(), 'master')
        self.assertEqual((source/'firmware.c').read_text(), 'master\n')
        self.assertEqual(git(source, 'rev-parse', 'HEAD').stdout.strip(), commit)

    def test_switch_preserves_nonconflicting_edits(self):
        (self.root/'operator-notes.txt').write_text('keep\n')
        source, _, _ = self.select('master')
        self.assertEqual((source/'operator-notes.txt').read_text(), 'keep\n')

    def test_conflicting_edits_stop_without_stash_or_discard(self):
        (self.root/'firmware.c').write_text('local timing fix\n')
        with self.assertRaisesRegex(ValueError, 'Branch switch failed'):
            self.select('master')
        self.assertEqual((self.root/'firmware.c').read_text(), 'local timing fix\n')
        self.assertEqual(git(self.root, 'branch', '--show-current').stdout.strip(), 'cmsis-ll-port')
        self.assertFalse(git(self.root, 'stash', 'list').stdout)

    def test_dry_run_does_not_switch(self):
        _, _, native = self.select('master', dry=True)
        self.assertFalse(native)
        self.assertEqual(git(self.root, 'branch', '--show-current').stdout.strip(), 'cmsis-ll-port')

    def test_cached_remote_branch_creates_local_tracking_branch(self):
        git(self.root, 'remote', 'add', 'origin', str(self.root))
        sha = git(self.root, 'rev-parse', 'master').stdout.strip()
        git(self.root, 'update-ref', 'refs/remotes/origin/remote-test', sha)
        _, commit, native = self.select('remote-test')
        self.assertEqual(commit, sha)
        self.assertFalse(native)
        self.assertEqual(git(self.root, 'branch', '--show-current').stdout.strip(), 'remote-test')

    def test_unknown_dry_run_and_invalid_branch_do_not_mutate(self):
        self.assertIsNone(self.select('uncached', dry=True))
        with self.assertRaises(ValueError):
            self.select('--force')
        self.assertEqual(git(self.root, 'branch', '--show-current').stdout.strip(), 'cmsis-ll-port')


if __name__ == '__main__':
    unittest.main()
