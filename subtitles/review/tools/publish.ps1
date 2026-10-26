# Publishes the reviewed subtitle data into the module's shipped data, inside this repository (byte-identical blobs):
#   subtitles/review/FLSubtitles/data/subtitles.tsv  -> subtitles/data/subtitles.tsv
#   subtitles/review/FLSubtitles/data/movies.tsv     -> subtitles/data/movies.tsv
#   subtitles/review/FLSubtitles/data/word_times.tsv -> subtitles/data/word_times.tsv (word timings, aligner/align_words.py)
# The source is the review product as STAGED (the index), so the pre-commit hook (tools/install-hooks.ps1) runs it and
# the commit that changes a review product carries its published copy too. Run by hand: stage the review products,
# then this script (it stages the copies), then commit.
# The copies in subtitles/data are never edited by hand: the script refuses to overwrite local edits there.
#   -Check    dry run: report what would be published, write nothing (exit 1 when something differs)
#   -NoStage  write the copies but do not stage them (the hook in a `git commit -- <paths>` commit: exit 2 when a
#             copy had to be written, so the commit is refused and redone with those paths)
param([switch]$Check, [switch]$NoStage, [switch]$Quiet)
$ErrorActionPreference = 'Stop'

$repo = (git -C $PSScriptRoot rev-parse --show-toplevel).Trim()
$names = 'subtitles.tsv', 'movies.tsv', 'word_times.tsv'
$pending = @(); $written = @()
foreach ($n in $names) {
    $src = "subtitles/review/FLSubtitles/data/$n"
    $dst = "subtitles/data/$n"
    $blob = (git -C $repo rev-parse ":$src").Trim()
    $staged = (git -C $repo rev-parse ":$dst" 2>$null)
    $head = (git -C $repo rev-parse "HEAD:$dst" 2>$null)
    $file = Join-Path $repo $dst
    $work = if (Test-Path $file) { (git -C $repo hash-object -- $dst).Trim() } else { '' }
    if ($staged -and $staged.Trim() -eq $blob -and $work -eq $blob) { continue }
    # anything in subtitles/data that is neither the committed copy nor the review product is a hand edit
    foreach ($v in @($staged, $work)) {
        if ($v -and $v.Trim() -ne $blob -and $v.Trim() -ne "$head".Trim()) {
            throw "$dst has local edits (it is only written by subtitles\review\tools\publish.ps1); revert them first"
        }
    }
    $pending += $dst
    if ($Check) { continue }
    if ($work -ne $blob) {
        # binary-safe copy of the blob (PowerShell pipes would re-encode the text)
        cmd /c "git -C `"$repo`" cat-file blob $blob > `"$file`""
        if ($LASTEXITCODE) { throw "could not write $file" }
        $written += $dst
    }
    if ($NoStage) { continue }
    git -C $repo add -- $dst
    $now = (git -C $repo rev-parse ":$dst").Trim()
    if ($now -ne $blob) { throw "$dst differs from $src after staging ($now vs $blob)" }
}
if (-not $pending) { if (-not $Quiet) { 'subtitles\data already matches the review products' }; exit 0 }
if ($Check) { "would publish: $($pending -join ', ')"; exit 1 }
if ($NoStage) { "published into the working tree (not staged): $($pending -join ', ')"; exit 2 }
"published and staged: $($pending -join ', ')"
exit 0
