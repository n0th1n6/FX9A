"""Select a firmware branch in the caller's existing worktree."""
import shlex
import subprocess


def git(root, *args, check=True):
    return subprocess.run(['git', '-C', str(root), *args], check=check,
                          capture_output=True, text=True)


def select_branch(root, branch, dry_run=False):
    if git(root, 'check-ref-format', '--branch', branch, check=False).returncode:
        raise ValueError('Supply a branch name, such as master or cmsis-ll-port.')
    commit = None
    local = False
    for ref in ('refs/heads/' + branch, 'refs/remotes/origin/' + branch):
        result = git(root, 'rev-parse', '--verify', ref + '^{commit}', check=False)
        if result.returncode == 0:
            commit = result.stdout.strip()
            local = ref.startswith('refs/heads/')
            break
    if commit is None:
        command = ['git', '-C', str(root), 'fetch', 'origin',
                   'refs/heads/' + branch + ':refs/remotes/origin/' + branch]
        print('+ ' + shlex.join(command), flush=True)
        if dry_run:
            print('Branch is not cached locally; fetch it before previewing the build.')
            return None
        subprocess.run(command, check=True)
        commit = git(root, 'rev-parse', '--verify',
                     'refs/remotes/origin/' + branch + '^{commit}').stdout.strip()
    current = git(root, 'symbolic-ref', '--quiet', '--short', 'HEAD', check=False)
    if current.returncode != 0 or current.stdout.strip() != branch:
        command = ['git', '-C', str(root), 'switch']
        command += ['--', branch] if local else ['--track', '-c', branch, 'origin/' + branch]
        print('+ ' + shlex.join(command), flush=True)
        if not dry_run:
            result = subprocess.run(command, capture_output=True, text=True)
            if result.returncode:
                raise ValueError('Branch switch failed; no build or flash was started. '
                                 'Local edits were not stashed or discarded.\n' + result.stderr.strip())
            print(result.stdout + result.stderr, end='')
    # In an actual run, inspect the selected working files, including local edits.
    native = (git(root, 'cat-file', '-e', commit + ':cmsis_ll/CMakeLists.txt', check=False).returncode == 0
              if dry_run and (current.returncode != 0 or current.stdout.strip() != branch)
              else (root/'cmsis_ll/CMakeLists.txt').is_file())
    print(f'Branch: {branch} ({commit}); source worktree: {root}; local edits are included.')
    return root, commit, native
