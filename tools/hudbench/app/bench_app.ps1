# The design bench as one application (the owner, 13 Nov 2026: "extremely easy to start and close, with 0 need for
# user-operated scripts and prompts"). The shortcut `HUD 2 Bench` (make_shortcut.ps1) runs this with no window of its
# own (conhost --headless). It:
#   1. builds the bench program (build\bench\flbench.exe) first when it is missing or older than its sources, with the
#      bench's window saying "building" meanwhile and showing the build's last lines if it fails;
#   2. starts `flbench serve` (127.0.0.1 only, a free port: tools\hudbench\serve.cpp);
#   3. opens the window in the user's default browser (browser.ps1; the owner's word of 13 Nov 2026: "Prefer default
#      browser"; none found: Chrome, Brave, Vivaldi, Firefox or Edge, the first installed). A Chromium browser gets an
#      app-mode window with a profile of its own for this start (build\bench\app\browser_<pid>), so its process can be
#      waited on and two starts give two independent windows; any other browser an ordinary tab;
#   4. ends when the window is gone: the browser's process ended (app mode), or the server ended itself because the page
#      said goodbye or stopped pinging (serve.cpp: whichever comes first; it also ends when this script or the window's
#      process ends). Then it ends the server and every process started with that profile, and deletes the profile.
# Nothing is installed and no setting of the machine is changed. What it did: build\bench\app\launch_<pid>.log (kept until
# the next start). Tests and agents use `flbench replay` / show.ps1 instead (README.md).
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'browser.ps1')
$root = Split-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) -Parent
$bench = Join-Path $root 'build\bench'
$app = Join-Path $bench 'app'
New-Item -ItemType Directory -Force $app | Out-Null
# the logs of starts whose launcher is gone
Get-ChildItem $app -Filter 'launch_*.log' -ErrorAction SilentlyContinue | Where-Object {
    $id = [int]($_.BaseName -replace '^launch_', ''); -not (Get-Process -Id $id -ErrorAction SilentlyContinue)
} | ForEach-Object { [IO.File]::Delete($_.FullName) }
$log = Join-Path $app "launch_$PID.log"
function Say([string]$s) { Add-Content -Path $log -Value ("{0:HH:mm:ss.fff} {1}" -f (Get-Date), $s) }

$exe = Join-Path $bench 'flbench.exe'
$profileDir = Join-Path $app "browser_$PID"
$status = Join-Path $app "status_$PID.html"
$portfile = Join-Path $app "port_$PID.txt"
$browser = $null
$browserProc = $null
$server = $null

function Write-Status([string]$title, [string]$body, [string]$refresh = '1') {
    $html = "<!doctype html><html><head><meta charset='utf-8'><meta http-equiv='refresh' content='$refresh'><title>HUD 2 Bench</title>" +
            "<style>body{font:15px system-ui,'Segoe UI',sans-serif;background:#14171c;color:#e3e7ee;margin:40px}h1{font-size:20px}" +
            "pre{background:#1c2027;padding:12px;border-radius:6px;white-space:pre-wrap;font-size:12px}</style></head>" +
            "<body><h1>$title</h1>$body</body></html>"
    [IO.File]::WriteAllText($status, $html)
}
function Open-Window([string]$url) {
    if ($browser.Kind -eq 'chromium') {
        $a = "--app=`"$url`" --user-data-dir=`"$profileDir`" --no-first-run --no-default-browser-check --disable-sync " +
             "--disable-background-timer-throttling --disable-renderer-backgrounding --disable-backgrounding-occluded-windows " +
             "--disable-features=Translate --window-size=1600,1000"
    } else { $a = "`"$url`"" }
    Say "window: $($browser.Exe) $a"
    $p = Start-Process -FilePath $browser.Exe -ArgumentList $a -PassThru
    # (app mode: the server watches the window's process too)
    if ($browser.Kind -eq 'chromium') { [IO.File]::WriteAllText("$portfile.window", "$($p.Id)`n") }
    $p
}
# every process started with this start's profile, whatever its name (the browser and its helpers)
function Get-ProfileProcesses {
    if ($browser.Kind -ne 'chromium') { return @() }
    @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object { $_.CommandLine -and $_.CommandLine.Contains($profileDir) })
}
# until the window is gone: the browser's process ended (app mode; or a process of the same profile it handed over to),
# or the server ended itself (the page said goodbye or stopped pinging)
function Wait-Window {
    while ($true) {
        if ($server -and $server.HasExited) { Say 'the server ended (the page closed or stopped pinging)'; return }
        if ($browserProc -and $browser.Kind -eq 'chromium') {
            if (-not $browserProc.WaitForExit(500)) { continue }
            $main = @(Get-ProfileProcesses | Where-Object { $_.CommandLine -notmatch '--type=' })
            if (-not $main.Count) { Say 'the window closed'; return }
            Say "the browser handed over to $($main[0].ProcessId)"
            $browserProc = Get-Process -Id $main[0].ProcessId -ErrorAction SilentlyContinue
            if (-not $browserProc) { return }
        }
        elseif (-not $server) { return } # (a tab with no server behind it: nothing to wait for)
        else { $null = $server.WaitForExit(1000) }
    }
}
function Stop-All {
    if ($server -and -not $server.HasExited) { Say 'ending the server'; Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue }
    foreach ($p in Get-ProfileProcesses) { Say "ending $($p.Name) $($p.ProcessId)"; Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue }
    for ($i = 0; $i -lt 20 -and (Test-Path $profileDir); $i++) {
        try { [IO.Directory]::Delete($profileDir, $true) } catch { Start-Sleep -Milliseconds 250 }
    }
    foreach ($f in $status, $portfile, "$portfile.window", (Join-Path $app "server_$PID.log"), (Join-Path $app "server_$PID.log.err")) {
        if (Test-Path $f) { [IO.File]::Delete($f) }
    }
    Say 'ended'
}

try {
    Say "start: root $root"
    $browser = Find-BenchBrowser
    if (-not $browser) {
        Say 'no browser found'
        Add-Type -AssemblyName System.Windows.Forms
        [Windows.Forms.MessageBox]::Show("HUD 2 Bench opens in a web browser (Google Chrome, Brave, Vivaldi, Firefox or Microsoft Edge), and none was found on this machine.", 'HUD 2 Bench') | Out-Null
        return
    }
    Say "browser: $($browser.Exe) ($($browser.Kind))"
    # 1. the program, built when it is missing or older than its sources (the files `build.cmd bench` compiles and their headers)
    $sources = @(Get-ChildItem -File -Path (@('tools\hudbench', 'modules\hud\src', 'core', 'core\ui', 'core\game') | ForEach-Object { Join-Path $root "$_\*" }) `
                     -Include '*.cpp', '*.h' -ErrorAction SilentlyContinue) + @(Get-Item (Join-Path $root 'build.cmd'))
    $newest = ($sources | Measure-Object -Property LastWriteTimeUtc -Maximum).Maximum
    $stale = -not (Test-Path $exe) -or (Get-Item $exe).LastWriteTimeUtc -lt $newest
    if ($stale) {
        Say 'building the bench program'
        Write-Status 'HUD 2 Bench: building' '<p>The bench program is being built from the current HUD code. This takes about half a minute, once.</p>'
        $browserProc = Open-Window ([Uri]$status).AbsoluteUri
        $buildLog = Join-Path $app "build_$PID.log"
        & (Join-Path $root 'tools\buildlock.ps1') bench *> $buildLog
        $code = $LASTEXITCODE
        Say "build exit $code"
        if ($code -ne 0 -or -not (Test-Path $exe)) {
            $tail = (Get-Content $buildLog -Tail 25 -ErrorAction SilentlyContinue) -join "`n"
            Write-Status 'HUD 2 Bench: the build failed' ("<p>The bench program did not build from the current HUD code. Its last lines:</p><pre>" +
                [Net.WebUtility]::HtmlEncode($tail) + "</pre><p>Close this window. The whole build output: " + [Net.WebUtility]::HtmlEncode($buildLog) + "</p>") '86400'
            Wait-Window
            return
        }
        [IO.File]::Delete($buildLog)
    }
    # 2. the server
    if (Test-Path $portfile) { [IO.File]::Delete($portfile) }
    $serverLog = Join-Path $app "server_$PID.log"
    $server = Start-Process -FilePath $exe -ArgumentList "serve --parent $PID --portfile `"$portfile`"" -NoNewWindow -PassThru `
                            -RedirectStandardOutput $serverLog -RedirectStandardError "$serverLog.err"
    $port = ''
    for ($i = 0; $i -lt 600 -and -not $port; $i++) {
        if (Test-Path $portfile) { $port = (Get-Content $portfile -Raw).Trim() }
        elseif ($server.HasExited) { break }
        else { Start-Sleep -Milliseconds 50 }
    }
    if (-not $port) {
        $why = ((Get-Content "$serverLog.err" -ErrorAction SilentlyContinue) + (Get-Content $serverLog -ErrorAction SilentlyContinue)) -join "`n"
        Say "the server did not start: $why"
        $server = $null
        Write-Status 'HUD 2 Bench: the bench program did not start' ("<pre>" + [Net.WebUtility]::HtmlEncode($why) + "</pre><p>Close this window.</p>") '86400'
        if (-not $browserProc) { $browserProc = Open-Window ([Uri]$status).AbsoluteUri }
        Wait-Window
        return
    }
    $url = "http://127.0.0.1:$port/"
    Say "server $($server.Id) at $url"
    # 3. the window (or the "building" page sent on to the bench)
    if ($browserProc) { Write-Status 'HUD 2 Bench' "<p>Opening the bench...</p>" "0;url=$url" }
    else { $browserProc = Open-Window $url }
    Say "window process $($browserProc.Id)"
    Wait-Window
}
catch { Say "error: $_" }
finally { Stop-All }
