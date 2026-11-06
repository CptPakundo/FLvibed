# The options page and the modules (the owner's O4, O5, O6). Row `alone` (subtitles alone): a row that acts stays live and saves, a row of an off module shows its file's
# value and names the module; the page sets a [Modules] row and writes only that; the second of subtitles / controller cannot go off. Row `pad` (all on): the controller
# turned off writes the rows it would have taken along; Reset Everything brings every row back alike. Row `stopped`: a stopped module's rows say so.
# The file's values are written while the game runs: deploy makes an instance's ini the main install's before a start, and the page reads a file when it lists rows.
# Usage (PowerShell tool): tests\page_modules.ps1 [-Row alone,pad,stopped] [-Instance 1]    build\release. Exit 1 on a failure.
param([string[]]$Row = @('alone', 'pad', 'stopped'), [int]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$deploy = Join-Path $root 'tools\deploy.ps1'
$bad = [Collections.Generic.List[string]]::new()
function Check($ok, [string]$what) { $ok = [bool]@($ok | Where-Object { $_ }).Count; "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Stop-Game { Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue; Get-FLGameProcess | Wait-Process -Timeout 20 -ErrorAction SilentlyContinue }
function Settle { Start-Sleep -Milliseconds 350 }
$script:alone = $true # `ui` is on the subtitles' channel with the controller off, on its own with it on
function Page([string]$c) { $r = if ($script:alone) { "$(Send-FLS "ui $c")" } else { "$(Send-FL "ui $c")" }; Settle; $r }
function Row-Of([string]$rows, [string]$key) { ($rows -split "`n" | Where-Object { $_ -match "^[> ]\s*$([regex]::Escape($key)) =" } | Select-Object -First 1) }
function Ini([string]$file, [string]$key) { $m = Select-String -LiteralPath (Join-Path (Get-FLInstallDir) "EXE\$file") -Pattern "^\s*$([regex]::Escape($key))\s*=\s*(.*)$" | Select-Object -First 1; if ($m) { $m.Matches[0].Groups[1].Value.Trim() } else { $null } }
function Set-Ini([string]$key, [string]$value) {
    $p = Join-Path (Get-FLInstallDir) 'EXE\FLController.ini'
    $t = [IO.File]::ReadAllText($p)
    $n = [regex]::Replace($t, "(?m)^(\s*$([regex]::Escape($key))\s*=\s*).*?(\r?)$", { param($x) $x.Groups[1].Value + $value + $x.Groups[2].Value })
    if ($n -eq $t) { throw "no row $key in the file" }
    [IO.File]::WriteAllText($p, $n)
}

try {
    foreach ($r in $Row) {
        "--- $r"
        $script:alone = $r -eq 'alone'
        if ($r -eq 'alone') {
            & $deploy -Background -NoBuild -Release -Instance $Instance -Modules subtitles -Save SaveC702.fl | Select-Object -Last 1
            Start-Sleep -Seconds 2
            foreach ($kv in @(@('rumble', '33'), @('borderless', '2'), @('autopilot_key', 'F10'))) { Set-Ini $kv[0] $kv[1] }
            $null = Page 'options open'
            $null = Page 'tab 1'
            $rows = Page 'rows'
            $line = Row-Of $rows 'rumble'; Check ($line -match '= 33 ' -and $line -match 'note=CONTROLLER MODULE OFF') "a pad row: $("$line".Trim())"
            $line = Row-Of $rows 'borderless'; Check ($line -match '= 2 ' -and $line -match 'note=DISPLAY MODULE OFF') "the Display's: $("$line".Trim())"
            foreach ($k in 'text_size', 'deck_mode', 'maneuver_watchdog', 'fluf_windows') { $line = Row-Of $rows $k; Check ($line -and $line -notmatch 'note=') "core's, live: $("$line".Trim())" }
            $null = Page 'focus text_size'
            $before = Ini 'FLController.ini' 'text_size'
            $null = Page 'press right'
            $now = (Row-Of (Page 'rows') 'text_size') -replace '^.*= (\d+).*$', '$1'
            Check ($now -ne $before) "text_size moves on the page: $before -> $now"
            $null = Page 'tab 2'
            $auto = Page 'rows'
            $line = Row-Of $auto 'autopilot_key'; Check ($line -match '= F10' -and $line -match 'note=AUTOPILOT MODULE OFF') "the Autopilot's: $("$line".Trim())"
            Check (($auto -split "`n" | Where-Object { $_ -match 'note=AUTOPILOT MODULE OFF' }).Count -gt 3) 'the Autopilot tab says so'
            $null = Page 'tab 0'
            $gen = Page 'rows'
            foreach ($k in 'mod_subtitles', 'mod_controller', 'mod_hud', 'mod_autopilot', 'mod_speedrun', 'mod_display', 'mod_npc', 'mod_movies') { Check ([bool](Row-Of $gen $k)) "the page lists $k" }
            $null = Page 'focus mod_speedrun'
            $null = Page 'press a'
            Check ((Row-Of (Page 'rows') 'mod_speedrun') -match '= 1') 'the Speedrun row turns on'
            $null = Page 'focus mod_subtitles'
            $null = Page 'press a'
            Check ((Row-Of (Page 'rows') 'mod_subtitles') -match '= 1') 'the Subtitles row stays on with the Controller off'
            $null = Page 'options close'
            Check ((Ini 'FLvibed.ini' 'speedrun') -eq '1') 'the save writes the row'
            Check ($null -eq (Ini 'FLvibed.ini' 'hud') -and $null -eq (Ini 'FLvibed.ini' 'movies')) 'and only that row: the others stay absent'
            Check ((Ini 'FLController.ini' 'text_size') -eq $now) "text_size is in the file: $(Ini 'FLController.ini' 'text_size')"
            Check ((Ini 'FLController.ini' 'rumble') -eq '33') 'the pad rows stay as they are in the file'
        } elseif ($r -eq 'pad') {
            & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,hud=0' -Save SaveC702.fl | Select-Object -Last 1
            Start-Sleep -Seconds 2
            $null = Page 'options open'
            $null = Page 'tab 0'
            $null = Page 'focus mod_controller'
            $null = Page 'press a'
            $null = Page 'options close'
            $vals = 'controller', 'hud', 'autopilot', 'speedrun', 'display', 'npc' | ForEach-Object { "$_=$(Ini 'FLvibed.ini' $_)" }
            Check ("$vals" -eq 'controller=0 hud=0 autopilot=1 speedrun=1 display=1 npc=1') "the controller off takes nothing along: $vals"
            Check ($null -eq (Ini 'FLvibed.ini' 'movies')) 'movies, inherited from the subtitles, stays absent'
            $null = Page 'options open'
            $null = Page 'tab 0'
            $null = Page 'focus reset_all'
            $null = Page 'press a'
            Start-Sleep -Milliseconds 500
            $gen = Page 'rows'
            foreach ($k in 'mod_controller', 'mod_hud', 'mod_autopilot', 'mod_speedrun', 'mod_display', 'mod_npc', 'mod_movies') { Check ((Row-Of $gen $k) -match '= 1') "Reset Everything: $k on" }
            $null = Page 'options close'
            $vals = 'controller', 'hud', 'autopilot' | ForEach-Object { "$_=$(Ini 'FLvibed.ini' $_)" }
            Check ("$vals" -eq 'controller=1 hud=1 autopilot=1') "and saved: $vals"
        } else {
            & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller' -Save SaveC702.fl | Select-Object -Last 1
            Start-Sleep -Seconds 2
            $null = Send-FL 'fault autopilot draw'
            Start-Sleep -Seconds 1
            $null = Page 'options open'
            $null = Page 'tab 2' # (`ui rows`' reply ends at its buffer: a tab's first rows are in it)
            $line = Row-Of (Page 'rows') 'auto_fire'
            Check ($line -match 'note=MODULE STOPPED') "a stopped module's row: $("$line".Trim())"
            $null = Page 'options close'
        }
        Stop-Game
    }
} finally { Stop-Game }
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'the page and the modules: PASS'
