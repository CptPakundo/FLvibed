"""Scripts naming a repository path that is not there. A move leaves these behind and nothing fails: `if (Test-Path $src)` just skips the copy (P11b:
deploy.ps1 kept reading modules\\controller\\data\\ after autopilot.3db moved, so new test installs had no bar mesh). Reads the live lines of the tracked
.ps1, .cmd and .rc files, comments left out; a path built from variables is not judged. Exit 1 on any dead path.

Usage: research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\path_check.py
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOP = r'(?:modules|core|tools|docs|tests|server|subtitles)'
PATH = re.compile(r'''(?<![\w.\\/$%-])(''' + TOP + r'''[\\/]+[\w.\\/-]+)''')
# git-ignored or made at run time: named by the scripts that make them
MADE = ('docs/kb/generated', 'server/accts')


def main():
    files = subprocess.run(['git', '-C', ROOT, 'ls-files'], capture_output=True, text=True).stdout.split('\n')
    tracked = {f.lower() for f in files if f}
    dirs = set()
    for f in tracked:
        while '/' in f:
            f = f.rsplit('/', 1)[0]
            dirs.add(f)
    dead = []
    for name in files:
        if not name.endswith(('.ps1', '.cmd', '.rc')) or name.startswith(('tools/claude/', 'subtitles/review/')):
            continue
        with open(os.path.join(ROOT, name), encoding='utf-8', errors='replace') as src:
            lines = src.read().split('\n')
        for n, line in enumerate(lines, 1):
            code = line.lstrip()
            if code.startswith(('#', 'rem ', '//', '::')):
                continue
            code = line.split(' # ')[0]
            for m in PATH.finditer(code):
                p = re.sub(r'/+', '/', m.group(1).replace('\\', '/')).lower()
                if code[m.end():m.end() + 1] in ('$', '*'):
                    p = p.rsplit('/', 1)[0]  # the name is built: its folder is what can be judged
                p = p.rstrip('/.')
                if p in tracked or p in dirs or p.startswith(MADE):
                    continue
                dead.append(f'{name}:{n}: {m.group(1)}')
    for d in dead:
        print(d)
    print(f'path_check: {len(dead)} dead path(s)')
    return 1 if dead else 0


if __name__ == '__main__':
    sys.exit(main())
