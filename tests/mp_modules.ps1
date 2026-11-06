# The server's rule reaches the Autopilot without the Controller (core\host\chatlines.*, core\game\mprules.*): a local server (tools\mptest), the plugin's word in
# EXE\config\flvibed.json, a client with `subtitles = 1, controller = 0, autopilot = 1`, driven through the verb `core` since its controller's channel does not
# exist. Rows: `travel` and `off` alone, `travel` with the controller, for the same answers. Judged: `mp` reads the server's word; a combat start is refused on
# travel, every start on off; a travel-class start flies on travel. Instance 21 (never the owner's copies).
# Usage (PowerShell tool): tests\mp_modules.ps1 [-Row alone-travel,alone-off,pad-travel] [-Instance 21]    build\release. Exit 1 on a failure.
param([string[]]$Row = @('alone-travel', 'alone-off', 'pad-travel'), [int]$Instance = 21)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\mptest\mptest.ps1')
$deploy = Join-Path $root 'tools\deploy.ps1'
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
$padSend = ${function:Send-FL}
$script:alone = $false
# mptest.ps1 drives the client through Send-FL: with the controller off the same verbs go through `core` on the subtitles' channel
function Send-FL([string]$Command, [int]$TimeoutMs = 5000) {
    if (-not $script:alone) { return & $padSend $Command $TimeoutMs }
    if ($Command -eq 'state') { $m = "$(Send-FLS 'mode' $TimeoutMs)"; return "mode=$m" + $(if ($m -match 'space|docked') { ' base=0x1 iobj=0x1' }) } # (`state` is the pad's verb)
    Send-FLS "core $Command" $TimeoutMs
}
function Stop-Client { Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue; Get-FLGameProcess | Wait-Process -Timeout 20 -ErrorAction SilentlyContinue }

$config = Join-Path (Get-FLMpExeDir) 'config\flvibed.json'
$kept = Get-Content -LiteralPath $config -Raw
try {
    foreach ($r in $Row) {
        "--- $r"
        $script:alone = $r -like 'alone*'
        $word = ($r -split '-')[1]
        Set-Content -LiteralPath $config -Value "{ `"autopilot`": `"$word`" }" -NoNewline
        Stop-FLMpServer | Out-Null
        Start-FLMpServer | Select-Object -Last 1
        Initialize-FLMpInstance | Out-Null
        $mods = if ($script:alone) { 'subtitles,autopilot=1' } else { 'subtitles,controller' }
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules $mods | Select-Object -Last 1
        Connect-FLMpServer | Select-Object -Last 1
        $until = (Get-Date).AddSeconds(20)
        do { Start-Sleep -Milliseconds 500; $mp = "$(if ($script:alone) { Send-FLS 'mp' } else { Send-FL 'mp' })" } while ($mp -notmatch 'the server says' -and (Get-Date) -lt $until)
        Check ($mp -match "multiplayer=1 autopilot: the server says $word") "the word: $(($mp -split "`n")[0])"
        $ap = { param($c) if ($script:alone) { Send-FLS "ap $c" } else { Send-FL "ap $c" } }
        $log = Join-Path (Get-FLInstallDir) $(if ($script:alone) { 'EXE\FLSubtitles.log' } else { 'EXE\FLController.log' })
        $null = & $ap 'start combat'
        Start-Sleep -Milliseconds 500
        $refused = @(Get-Content $log | Where-Object { $_ -match 'autopilot: refused combat' })
        Check ($refused.Count -ge 1) "a combat start is refused: $(($refused | Select-Object -Last 1) -replace '^.*autopilot: ', '')"
        $null = & $ap 'start hold'
        Start-Sleep -Milliseconds 500
        $held = @(Get-Content $log | Where-Object { $_ -match 'autopilot: refused hold' })
        if ($word -eq 'off') { Check ($held.Count -ge 1) 'every start is refused' } else { Check ($held.Count -eq 0) 'a travel start is not refused by the rule' }
        Stop-Client
    }
} finally {
    Stop-Client
    Stop-FLMpServer | Out-Null
    Set-Content -LiteralPath $config -Value $kept -NoNewline
}
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'the server''s rule with the controller off: PASS'
