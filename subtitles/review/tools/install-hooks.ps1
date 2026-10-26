# Installs FLvibed's local pre-commit hook (hooks are not versioned) with the subtitle review step chained after the
# privacy guard:
#   pre-commit.privacy - the privacy guard (refuses e-mail addresses, the dev user name, user-profile paths). An
#                        existing local guard (the old pre-commit) is kept as it is and only renamed; without one it
#                        is generated here.
#   pre-commit         - runs pre-commit.privacy, then, when a review product is staged
#                        (subtitles/review/FLSubtitles/data/subtitles.tsv | movies.tsv | word_times.tsv) or one of the
#                        published copies in subtitles/data:
#                          1. re-aligns the word timings when the review subtitles.tsv is staged
#                             (FLSubtitles/aligner/align_words.py, incremental) and stages word_times.tsv;
#                          2. publishes the review products into subtitles/data (tools/publish.ps1) and stages them,
#                             so the commit carries the review change and its published copy together.
#                        In a `git commit -- <paths>` commit the hook cannot stage extra files: it writes them and
#                        refuses the commit; commit again with those paths added.
# Run once per clone (idempotent). -ExtraWords: more personal words for a generated guard (e.g. a mail provider),
# never written to the repository.
param([string[]]$ExtraWords = @())
$ErrorActionPreference = 'Stop'
$repo = (git -C $PSScriptRoot rev-parse --show-toplevel).Trim()
$hooks = (git -C $repo rev-parse --absolute-git-dir).Trim() + '\hooks'
$lf = { param($s) $s.Replace("`r`n", "`n") }
$marker = '# FLvibed pre-commit chain (subtitles/review/tools/install-hooks.ps1)'
$pre = Join-Path $hooks 'pre-commit'
$guard = Join-Path $hooks 'pre-commit.privacy'

if ((Test-Path $pre) -and -not (Select-String -Path $pre -SimpleMatch $marker -Quiet)) {
    if (Test-Path $guard) { throw "both $pre (not ours) and $guard exist; merge them by hand" }
    Move-Item $pre $guard
    "kept the existing pre-commit as pre-commit.privacy"
}
if (-not (Test-Path $guard)) {
    # The personal words are filled in at install time, so this script holds no personal data: this machine's user
    # name, -ExtraWords and the mail provider of the global git identity.
    $words = @($env:USERNAME.ToLower()) + $ExtraWords
    $mail = (git config --global user.email 2>$null)
    if ($mail -match '@([^.]+)\.') { $words += $Matches[1].ToLower() }
    $personal = ($words | Where-Object { $_ } | ForEach-Object { $_.ToLower() -replace '[^a-z0-9._-]', '' } |
        Select-Object -Unique) -join '|'
    $g = @'
#!/bin/sh
# Privacy guard (local only): refuse commits whose added lines contain personal details:
# e-mail addresses, the dev machine's user name, or Windows user-profile paths.
# Allowed addresses: the neutral repo identity and the Claude co-author trailer.
hits=$(git diff --cached -U0 --no-color | grep '^+' | grep -v '^+++' \
  | grep -i -E 'PERSONAL|[A-Za-z]:.Users.|[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+[.][A-Za-z]{2,}' \
  | grep -v -E 'noreply@anthropic[.]com|dev@flcontroller[.]invalid|dev@flsubtitles[.]invalid|dev@flvibed[.]invalid')
if [ -n "$hits" ]; then
  echo "pre-commit: personal details (e-mail / user name / user path) in staged changes:" >&2
  echo "$hits" | head -20 >&2
  exit 1
fi
exit 0
'@
    [IO.File]::WriteAllText($guard, (& $lf $g.Replace('PERSONAL', $personal)))
    "generated pre-commit.privacy"
}

$chain = @'
#!/bin/sh
MARKER
# 1. the privacy guard (local, not versioned)
"$(dirname "$0")/pre-commit.privacy" "$@" || exit 1

# 2. the subtitle review products (subtitles/review/CLAUDE.md "Publishing")
R=subtitles/review/FLSubtitles
staged=$(git diff --cached --name-only)
echo "$staged" | grep -q -E "^($R/data/(subtitles|movies|word_times)|subtitles/data/(subtitles|movies|word_times))[.]tsv$" || exit 0
# a `git commit -- <paths>` commit runs on a temporary index: files the hook writes cannot be staged into it
partial=0
case "$GIT_INDEX_FILE" in *next-index*) partial=1 ;; esac
# changed subtitle text: its word timings are aligned again (user, 22 Oct 2026: "anything that was changed would need
# to have its forced alignment re-run"; incremental) and committed with it
if echo "$staged" | grep -q -E "^$R/data/subtitles[.]tsv$"; then
  py=$R/aligner/.venv/Scripts/python.exe
  if [ ! -x "$py" ]; then
    echo "pre-commit: subtitles.tsv changed but the aligner is not set up (cd $R/aligner; uv sync)" >&2
    exit 1
  fi
  log=$R/out/align_words.log
  mkdir -p $R/out
  if "$py" -I $R/aligner/align_words.py > "$log" 2>&1; then
    grep -E '^[0-9]+ lines|^done' "$log" >&2
  else
    echo "pre-commit: word alignment FAILED (see $log)" >&2
    exit 1
  fi
  if [ $partial = 1 ]; then
    if ! git diff --quiet -- $R/data/word_times.tsv; then
      echo "pre-commit: word_times.tsv was re-aligned; commit again with $R/data/word_times.tsv in the paths" >&2
      exit 1
    fi
  else
    git add $R/data/word_times.tsv
  fi
fi
ps=pwsh; command -v pwsh >/dev/null 2>&1 || ps=powershell
if [ $partial = 1 ]; then
  "$ps" -NoProfile -ExecutionPolicy Bypass -File subtitles/review/tools/publish.ps1 -NoStage -Quiet >&2
  rc=$?
  if [ $rc = 2 ]; then echo "pre-commit: commit again with subtitles/data/subtitles.tsv movies.tsv word_times.tsv in the paths" >&2; exit 1; fi
  [ $rc = 0 ] || { echo "pre-commit: publishing the subtitle data FAILED" >&2; exit 1; }
else
  "$ps" -NoProfile -ExecutionPolicy Bypass -File subtitles/review/tools/publish.ps1 -Quiet >&2 \
    || { echo "pre-commit: publishing the subtitle data FAILED" >&2; exit 1; }
fi
exit 0
'@
[IO.File]::WriteAllText($pre, (& $lf $chain.Replace('MARKER', $marker)))
"installed pre-commit (privacy guard + subtitle review step) in $hooks"
