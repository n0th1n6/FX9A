#!/usr/bin/env python3
"""Push a committed branch from this worktree to origin and verify its remote SHA."""
import argparse
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
from branch_checkout import git, select_branch

ROOT = Path(__file__).resolve().parents[1]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--branch', required=True, help='branch to switch to and push to origin')
    parser.add_argument('--dry-run', action='store_true', help='preview without switching branches or pushing')
    args = parser.parse_args(argv)
    try:
        if git(ROOT, 'status', '--porcelain').stdout:
            raise ValueError('Worktree has uncommitted changes. Commit the intended files before pushing.')
        selected = select_branch(ROOT, args.branch, args.dry_run)
        if selected is None:
            return 0
        _, commit, _ = selected
        ref = 'refs/heads/' + args.branch
        print(f'Push: {commit} -> origin/{args.branch}', flush=True)
        if args.dry_run:
            return 0
        # Normal push only: never force, stage files, create commits or merge.
        subprocess.run(['git', '-C', str(ROOT), 'push', '--set-upstream', 'origin',
                        f'{ref}:{ref}'], check=True)
        remote = git(ROOT, 'ls-remote', '--exit-code', 'origin', ref).stdout.split()
        if len(remote) != 2 or remote != [commit, ref]:
            raise ValueError('Remote branch SHA does not match the selected commit.')
        if git(ROOT, 'status', '--porcelain').stdout:
            raise ValueError('Push completed, but the worktree changed during publication.')
        print(f'Pushed and verified {args.branch}: {commit}; worktree clean.')
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError, KeyboardInterrupt) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
