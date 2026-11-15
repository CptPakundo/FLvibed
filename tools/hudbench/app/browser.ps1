# The browser the bench's window opens in (dot-sourced by bench_app.ps1 and make_shortcut.ps1; test_replayer.js has the
# same order for its headless run). The owner's word, 13 Nov 2026: "Prefer default browser."
#   1. the user's default browser (the https UserChoice ProgId and its shell\open\command);
#   2. none found: the first installed of Chrome, Brave, Vivaldi, Firefox, Edge.
# Returns @{ Exe; Kind } with Kind 'chromium' (Chrome, Brave, Vivaldi, Chromium, Edge: an app-mode window with a profile
# of its own) or 'tab' (any other browser: an ordinary tab in it), or $null when none is found.
function Find-BenchBrowser {
    $chromium = '^(chrome|brave|vivaldi|chromium|msedge)\.exe$'
    $exe = $null
    try {
        $prog = (Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\Shell\Associations\UrlAssociations\https\UserChoice' -ErrorAction Stop).ProgId
        $cmd = (Get-ItemProperty "Registry::HKEY_CLASSES_ROOT\$prog\shell\open\command" -ErrorAction Stop).'(default)'
        if ($cmd -match '^\s*"([^"]+\.exe)"' -or $cmd -match '^\s*(\S+\.exe)') { $exe = $Matches[1] }
    } catch { $exe = $null }
    if ($exe -and (Test-Path $exe)) {
        return @{ Exe = $exe; Kind = $(if ((Split-Path $exe -Leaf) -match $chromium) { 'chromium' } else { 'tab' }) }
    }
    $pf = $env:ProgramFiles; $pf86 = ${env:ProgramFiles(x86)}; $local = $env:LOCALAPPDATA
    foreach ($p in "$pf\Google\Chrome\Application\chrome.exe", "$pf86\Google\Chrome\Application\chrome.exe", "$local\Google\Chrome\Application\chrome.exe",
                   "$pf\BraveSoftware\Brave-Browser\Application\brave.exe", "$local\BraveSoftware\Brave-Browser\Application\brave.exe",
                   "$local\Vivaldi\Application\vivaldi.exe", "$pf\Vivaldi\Application\vivaldi.exe") {
        if ($p -and (Test-Path $p)) { return @{ Exe = $p; Kind = 'chromium' } }
    }
    foreach ($p in "$pf\Mozilla Firefox\firefox.exe", "$pf86\Mozilla Firefox\firefox.exe") {
        if ($p -and (Test-Path $p)) { return @{ Exe = $p; Kind = 'tab' } }
    }
    foreach ($p in "$pf86\Microsoft\Edge\Application\msedge.exe", "$pf\Microsoft\Edge\Application\msedge.exe") {
        if ($p -and (Test-Path $p)) { return @{ Exe = $p; Kind = 'chromium' } }
    }
    return $null
}
