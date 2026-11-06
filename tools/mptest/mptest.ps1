# The local multiplayer test setup (tools\mptest\README.md) - dot-source after tools\harness\flctl.ps1 (PowerShell
# tool only). A test instance (never the main install, never v11nightly or another worker's copy) runs FLServer.exe
# and a test client on this machine; flmptest.dll (build.cmd mptest; TEST-ONLY) keeps the multiplayer account key and
# the server's data inside the instance (the owner, 28 Oct 2026: nothing outside the project, and always know which
# account key a test used and that the server's data path is not the game's default). Its ledger:
# <instance>\SAVE\mptest\mptest.log.
#
#   Initialize-FLMpInstance            the instance's dacom.ini / dacomsrv.ini list flmptest.dll first (copies, never
#                                      the main install's hard-linked files); the DLL copied in
#   Start-FLMpServer [-Account set]    FLServer.exe /c, hidden and windowless (FLVIBED_BACKGROUND), waits for its port
#   Stop-FLMpServer                    ends it
#   Get-FLMpAccounts                   the account sets on disk and which one the ledger used last
# The client side: `deploy.ps1 -Instance <n> -Background` as always; FLVIBED_MPACCOUNT (default "default") picks the
# account set in the client too (set it before the start; Start-FLMpServer sets it for its own process only).
#
# Several clients on one server (28 Oct 2026): the server runs on one instance, each client on its own instance (its
# own channel Local\FLControllerShm_<n>, logs, account sets under its own SAVE\mptest). Every harness call goes to the
# instance $env:FLVIBED_INSTANCE names:
#   Invoke-FLMpAt <n> { ... }           runs a block against instance n (the env restored after)
#   Start-FLMpClient <n> [-Account set] the client of instance n: flmptest.dll listed, deploy -Background -NoBuild
#                                       with its account set (default "client<n>": one identity per instance)
#   Start-FLMpSession -Server 21 -Clients 21,22 [-PvP]   server + clients + each connected under its own character
#   Stop-FLMpSession                    every client quits (Quit-FLGame: the server frees the account), the server ends

if (-not ('FLMpWin' -as [type])) {
    Add-Type @'
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public static class FLMpWin {
    delegate bool CB(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] static extern bool EnumWindows(CB cb, IntPtr l);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr p, CB cb, IntPtr l);
    [DllImport("user32.dll")] static extern int GetWindowThreadProcessId(IntPtr h, out int p);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] static extern int GetDlgCtrlID(IntPtr h);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, int m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, int m, IntPtr w, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern IntPtr SendMessage(IntPtr h, int m, IntPtr w, string l);
    public static void SetText(IntPtr h, string t) { SendMessage(h, 0x000C, IntPtr.Zero, t); } // WM_SETTEXT
    static string Text(IntPtr h) { var s = new StringBuilder(256); GetWindowText(h, s, 256); return s.ToString(); }
    static string Class(IntPtr h) { var s = new StringBuilder(256); GetClassName(h, s, 256); return s.ToString(); }
    // "hwnd|top|class|id|text|visible" per top-level window of the process and each of its controls
    public static List<string> List(int pid) {
        var o = new List<string>();
        EnumWindows((h, l) => { int p; GetWindowThreadProcessId(h, out p); if (p == pid) {
            o.Add(h.ToInt64() + "|top|" + Class(h) + "|0|" + Text(h) + "|" + IsWindowVisible(h));
            EnumChildWindows(h, (c, l2) => { o.Add(c.ToInt64() + "|" + h.ToInt64() + "|" + Class(c) + "|" + GetDlgCtrlID(c) + "|" + Text(c) + "|" + IsWindowVisible(c)); return true; }, IntPtr.Zero);
        } return true; }, IntPtr.Zero);
        return o;
    }
}
'@
}

# The server's windows and their controls (FLServer is a dialog program; a test drives it by its controls' ids).
function Get-FLMpServerWindows {
    $p = Get-FLMpServer
    if (-not $p) { return @() }
    [FLMpWin]::List($p.Id) | ForEach-Object {
        $f = $_ -split '\|', 6
        [pscustomobject]@{ Hwnd = [int64]$f[0]; Top = $f[1]; Class = $f[2]; Id = [int]$f[3]; Text = $f[4]; Visible = $f[5] }
    }
}

# Presses a server dialog's button by its text (BM_CLICK: the dialog's own handler, as a click does).
function Invoke-FLMpServerButton([string]$Text, [string]$Dialog) {
    $w = @(Get-FLMpServerWindows)
    $dlg = $w | Where-Object { $_.Top -eq 'top' -and $_.Text -eq $Dialog } | Select-Object -First 1
    if (-not $dlg) { throw "no server window '$Dialog' (windows: $(($w | Where-Object Top -eq 'top' | ForEach-Object Text) -join ', '))" }
    $b = $w | Where-Object { $_.Top -eq "$($dlg.Hwnd)" -and $_.Class -eq 'Button' -and ($_.Text -replace '&', '') -eq $Text } | Select-Object -First 1
    if (-not $b) { throw "no button '$Text' in '$Dialog'" }
    $null = [FLMpWin]::PostMessage([IntPtr]$b.Hwnd, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) # BM_CLICK
    "pressed '$Text' in '$Dialog'"
}

# FLServer's first run (no FLServer.cfg in its data folder): the "Server Configuration" dialog waits for OK even with
# /c. Filled through its controls: the name (RichEdit 1006), a description (1007), no password (1008), LAN only
# (1014 "Make Your Server Internet Accessible" unchecked), new players allowed (1010); then OK writes FLServer.cfg
# and the server starts. Later /c starts read the file and skip the dialog.
function Complete-FLMpServerConfig([string]$Name = 'FLvibed test server') {
    $w = @(Get-FLMpServerWindows)
    $dlg = $w | Where-Object { $_.Top -eq 'top' -and $_.Text -eq 'Server Configuration' } | Select-Object -First 1
    if (-not $dlg) { return 'no configuration dialog' }
    $c = { param($id) $w | Where-Object { $_.Top -eq "$($dlg.Hwnd)" -and $_.Id -eq $id } | Select-Object -First 1 }
    [FLMpWin]::SetText([IntPtr](& $c 1006).Hwnd, $Name)
    [FLMpWin]::SetText([IntPtr](& $c 1007).Hwnd, 'Local test server for FLvibed (LAN only)')
    $check = { param($id, [bool]$on)
        $h = [IntPtr](& $c $id).Hwnd
        $is = [FLMpWin]::SendMessage($h, 0x00F0, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -eq 1 # BM_GETCHECK
        if ($is -ne $on) { $null = [FLMpWin]::SendMessage($h, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) } } # BM_CLICK
    & $check 1014 $false
    & $check 1010 $true
    # "Allow Players to Harm Other Players" (PvP): found by its text, set as Start-FLMpSession -PvP asked
    $harm = $w | Where-Object { $_.Top -eq "$($dlg.Hwnd)" -and $_.Class -eq 'Button' -and $_.Text -match 'harm' } | Select-Object -First 1
    if ($harm) { & $check $harm.Id ([bool]$script:FLMpPvP) } else { Write-Warning "no 'harm other players' box in the dialog: $(($w | Where-Object Class -eq 'Button' | ForEach-Object { "$($_.Id)=$($_.Text)" }) -join '; ')" }
    Invoke-FLMpServerButton 'OK' 'Server Configuration'
}

function Get-FLMpExeDir { Join-Path (Get-FLInstallDir) 'EXE' }
function Get-FLMpRoot { Join-Path (Get-FLInstallDir) 'SAVE\mptest' }

# A hard-linked instance file is replaced by a private copy before it is changed (the main install keeps its bytes).
function Split-FLHardLink([string]$Path) {
    $item = Get-Item -LiteralPath $Path
    if ($item.LinkType -eq 'HardLink' -and @($item.Target).Count -gt 0) {
        $tmp = "$Path.copy"
        Copy-Item -LiteralPath $Path $tmp
        [IO.File]::Delete($Path)
        Move-Item -LiteralPath $tmp $Path
    }
}

# Lists flmptest.dll as the first [Libraries] entry of an ini (idempotent).
function Add-FLMpLibrary([string]$Ini) {
    Split-FLHardLink $Ini
    $lines = [Collections.Generic.List[string]](Get-Content -LiteralPath $Ini)
    if ($lines | Where-Object { $_.Trim() -ieq 'flmptest.dll' }) { return "$(Split-Path $Ini -Leaf): flmptest.dll already listed" }
    $i = $lines.FindIndex({ param($l) $l.Trim() -ieq '[Libraries]' })
    if ($i -lt 0) { throw "no [Libraries] in $Ini" }
    $lines.Insert($i + 1, 'flmptest.dll')
    Set-Content -LiteralPath $Ini $lines -Encoding ascii
    "$(Split-Path $Ini -Leaf): flmptest.dll listed (test-only)"
}

function Initialize-FLMpInstance {
    # a numbered instance (HDE mirrors) or a named one (vanilla_mp: the original 1.1, its "Freelancer HD Edition" a junction)
    if (-not $env:FLVIBED_INSTANCE -or $env:FLVIBED_INSTANCE -eq '0') { throw 'multiplayer tests run on a test instance: set $env:FLVIBED_INSTANCE (never the main install)' }
    if ((Get-FLInstallDir) -match 'v11nightly') { throw 'never the owner''s v11nightly instance' }
    $exe = Get-FLMpExeDir
    if (-not (Test-Path -LiteralPath (Join-Path $exe 'FLServer.exe'))) { throw "no FLServer.exe in $exe" }
    $build = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'build\mptest'
    foreach ($f in 'flmptest.dll', 'flmplaunch.exe') {
        if (-not (Test-Path -LiteralPath (Join-Path $build $f))) { throw "build\mptest\$f missing: build.cmd mptest" }
    }
    $to = Join-Path $exe 'flmptest.dll'
    $from = Join-Path $build 'flmptest.dll'
    $same = (Test-Path -LiteralPath $to) -and (Get-FileHash -LiteralPath $to).Hash -eq (Get-FileHash -LiteralPath $from).Hash
    if (-not $same) {
        # a fresh copy, never a link; a running client holds it (then the old build stays until its next start)
        try { if (Test-Path -LiteralPath $to) { [IO.File]::Delete($to) }; Copy-Item -LiteralPath $from $to }
        catch { Write-Warning "flmptest.dll in use (a running game): the instance keeps its copy until the next start" }
    }
    # the client: a dacom.ini library (Common.dll reads the account key long after DACOM's start); the server: never
    # in dacomsrv.ini - FLServer resolves its data folder before DACOM, so Start-FLMpServer loads the DLL first
    # (flmplaunch.exe)
    Add-FLMpLibrary (Join-Path $exe 'dacom.ini')
    # HDE's server plugin FLHook resolves the save folder itself: [DACOM] SavePath in dacom.ini, else it creates
    # "%UserProfile%\My Documents\My Games\Freelancer" (28 Oct 2026: an empty folder in Documents) - point it at the
    # server's data folder (the client ignores the key)
    $srv = Join-Path (Get-FLMpRoot) 'server'
    $null = New-Item -ItemType Directory -Force $srv
    $dacom = Join-Path $exe 'dacom.ini'
    $lines = [Collections.Generic.List[string]](Get-Content -LiteralPath $dacom)
    $k = $lines.FindIndex({ param($l) $l -match '^\s*SavePath\s*=' })
    if ($k -ge 0) { $lines[$k] = "SavePath = $srv" }
    else {
        $i = $lines.FindIndex({ param($l) $l.Trim() -ieq '[DACOM]' })
        if ($i -lt 0) { throw "no [DACOM] in $dacom" }
        $lines.Insert($i + 1, "SavePath = $srv")
    }
    Set-Content -LiteralPath $dacom $lines -Encoding ascii
}

function Get-FLMpServer {
    $exe = Join-Path (Get-FLMpExeDir) 'FLServer.exe'
    Get-Process FLServer -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path -ieq $exe } | Select-Object -First 1
}

# FLServer's UDP port(s) once it listens (DirectPlay: 2302 by default).
function Get-FLMpServerPorts {
    $p = Get-FLMpServer
    if (-not $p) { return @() }
    @(Get-NetUDPEndpoint -OwningProcess $p.Id -ErrorAction SilentlyContinue | Select-Object -ExpandProperty LocalPort | Sort-Object -Unique)
}

function Start-FLMpServer([string]$Account = 'default', [int]$TimeoutSec = 60) {
    Initialize-FLMpInstance | Out-Null
    if (Get-FLMpServer) { return "server already running (pid $((Get-FLMpServer).Id))" }
    $exe = Get-FLMpExeDir
    $old = @{ bg = $env:FLVIBED_BACKGROUND; acc = $env:FLVIBED_MPACCOUNT }
    try {
        $env:FLVIBED_BACKGROUND = '1'; $env:FLVIBED_MPACCOUNT = $Account
        $launch = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'build\mptest\flmplaunch.exe'
        $out = & $launch (Join-Path $exe 'flmptest.dll') $exe (Join-Path $exe 'FLServer.exe') '/c'
        if ($LASTEXITCODE -ne 0) { throw "flmplaunch failed: $out" }
        $p = Get-Process -Id ([int]$out)
        $null = $p.Handle # keeps the exit code readable
    } finally { $env:FLVIBED_BACKGROUND = $old.bg; $env:FLVIBED_MPACCOUNT = $old.acc }
    $t0 = Get-Date
    $configured = $false
    while (-not (Get-FLMpServerPorts | Where-Object { $_ -ge 2302 -and $_ -le 2400 })) {
        if (-not $configured -and (Get-FLMpServerWindows | Where-Object { $_.Top -eq 'top' -and $_.Text -eq 'Server Configuration' })) {
            Write-Host "first run: $(Complete-FLMpServerConfig)"; $configured = $true
        }
        if ($p.HasExited) { throw "FLServer exited ($('0x{0:X8}' -f $p.ExitCode)); ledger: $(Get-Content -LiteralPath (Join-Path (Get-FLMpRoot) 'mptest.log') -Tail 5 -ErrorAction SilentlyContinue)" }
        if (((Get-Date) - $t0).TotalSeconds -gt $TimeoutSec) { throw "FLServer (pid $($p.Id)) not listening after $TimeoutSec s; ports: $(Get-FLMpServerPorts)" }
        Start-Sleep -Milliseconds 500 # the server's own start: no game event to wait on from outside
    }
    "server pid $($p.Id) listening on UDP $((Get-FLMpServerPorts) -join ', ') after $([int]((Get-Date) - $t0).TotalSeconds) s (account set '$Account', data $(Get-FLMpRoot)\server)"
}

function Stop-FLMpServer {
    $p = Get-FLMpServer
    if (-not $p) { return 'no server' }
    Stop-Process -Id $p.Id -Force
    $null = $p.WaitForExit(10000)
    "server pid $($p.Id) stopped"
}

# The client's way in, every step native and on state (28 Oct 2026, HDE 0.7.1): main menu MULTIPLAYER -> the "LAN /
# INTERNET" prompt (a modal loop: PromptYes = LAN, never INTERNET - no global server contact) -> the account screen
# (only while the account set has no key: SelectServer) -> the server list (ServerTable: rows from 1, row 0 is never
# selectable; `table` selects natively) -> Connect -> the character list (CharacterTable) -> a character by name
# (NewCharacter -> CharacterName -> SelectShip = CREATE when it does not exist) -> LoadCharacter -> docked.
function Connect-FLMpServer([string]$Character = 'FLvibedTester', [int]$Row = 1, [int]$TimeoutSec = 90) {
    # this instance's server, or another test instance's (a client may join a server of another build: vanilla_mp's)
    if (-not (Get-FLMpServer) -and -not (Get-Process FLServer -ErrorAction SilentlyContinue)) { throw 'no test server: Start-FLMpServer first' }
    $null = Wait-FLUntil { (Send-FL 'uiitems') -match '(?m)^(FrontButton|ServerTable|SelectServer)\s' } 'the main menu, the account screen or the server list' 30
    if ((Send-FL 'uiitems') -notmatch '(?m)^(ServerTable|SelectServer)\s') {
        Send-FL 'menu multiplayer' | Out-Null
        $null = Wait-FLUntil { (Send-FL 'visible PromptYes') -match '^visible' } 'the LAN / INTERNET prompt' 15
        # a click while the prompt is still coming up is dropped (vanilla 1.1, 28 Oct 2026): again until it closes
        $null = Wait-FLUntil { if ((Send-FL 'visible PromptYes') -match '^visible') { Send-FL 'click PromptYes' | Out-Null; $false } else { $true } } 'LAN chosen' 15
    }
    $null = Wait-FLUntil { $u = Send-FL 'uiitems'; $u -match '(?m)^(SelectServer|ServerTable)\s' } 'the account screen or the server list' 20
    if ((Send-FL 'visible SelectServer') -match '^visible') { Send-FL 'click SelectServer' | Out-Null }
    $null = Wait-FLUntil { (Send-FL 'table ServerTable') -match 'rows=' -and (Send-FL "table ServerTable $Row") -match '^selected' } 'the local server in the list' 40
    $null = Wait-FLUntil { (Send-FL 'visible Connect') -match '^visible' } 'CONNECT enabled' 10
    Send-FL 'click Connect' | Out-Null
    # the server may refuse (NN_Error, ErrorOkayButton): "that account is already in use" after a client was killed
    # without leaving - the server holds the account until its timeout; restart the test server, or quit clients
    # gracefully (Quit-FLGame)
    $null = Wait-FLUntil { (Send-FL 'uiitems') -match '(?m)^(NewCharacter|ErrorOkayButton)\s' } 'the character list' 30
    if ((Send-FL 'visible ErrorOkayButton') -match '^visible') {
        Send-FL 'click ErrorOkayButton' | Out-Null
        throw 'the server refused the connection (NN_Error) - e.g. the account is still in use after a killed client: Stop-FLMpServer; Start-FLMpServer'
    }
    # the character: the table's rows carry no readable names yet, so the first row is the account's first character;
    # a fresh account set has none and gets one
    # the server's character list arrives after the screen (28 Oct 2026: a check at once found no row and tried to
    # create an existing character - "name in use"): up to 8 s for row 1 before calling the account empty
    $has = $false
    try { $null = Wait-FLUntil { (Send-FL 'table CharacterTable 1') -match '^selected' } 'a character in the list' 8; $has = $true } catch { }
    if (-not $has) {
        Send-FL 'click NewCharacter' | Out-Null
        $null = Wait-FLUntil { (Send-FL 'uiitems') -match '(?m)^CharacterName\s' } 'the new character screen' 15
        Send-FL "type $Character" | Out-Null
        $null = Wait-FLUntil { (Send-FL 'visible SelectShip') -match '^visible' } 'CREATE enabled' 10
        Send-FL 'click SelectShip' | Out-Null
        $null = Wait-FLUntil { (Send-FL 'table CharacterTable 1') -match '^selected' } 'the new character listed' 20
    }
    $null = Wait-FLUntil { (Send-FL 'visible LoadCharacter') -match '^visible' } 'LOAD enabled' 10
    Send-FL 'click LoadCharacter' | Out-Null
    # docked, or in space (FLHook keeps a character that left in flight where it was)
    $null = Wait-FLUntil { (Send-FL 'state') -match 'base=0x[1-9a-f]|iobj=0x[1-9a-f]' } 'docked or in space on the server' $TimeoutSec
    "connected: $((Send-FL 'state') -split "`n" | Select-String 'base=')"
}

function Invoke-FLMpAt([int]$Instance, [scriptblock]$Block) {
    $old = $env:FLVIBED_INSTANCE
    try { $env:FLVIBED_INSTANCE = "$Instance"; & $Block } finally { $env:FLVIBED_INSTANCE = $old }
}

# The client of instance $Instance, muted and in the background, with its own account set (a separate MP identity:
# the server keys accounts by it, and two clients on one account are refused "already in use").
function Start-FLMpClient([int]$Instance, [string]$Account = '') {
    if (-not $Account) { $Account = "client$Instance" }
    $deploy = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'tools\deploy.ps1'
    Invoke-FLMpAt $Instance {
        if (-not (Test-Path -LiteralPath (Join-Path (Get-FLInstallDir) 'EXE\Freelancer.exe'))) { $null = New-FLInstance $Instance }
        Initialize-FLMpInstance | Out-Null
        $old = $env:FLVIBED_MPACCOUNT
        try { $env:FLVIBED_MPACCOUNT = $Account; & $deploy -Instance $Instance -Background -NoBuild | Out-Null }
        finally { $env:FLVIBED_MPACCOUNT = $old }
        $p = Get-FLGameProcess
        if (-not $p) { throw "instance ${Instance}: the client did not start" }
        $null = Wait-FLUntil { (Send-FL 'uiitems') -match '(?m)^FrontButton' } 'the main menu' 60
        "instance ${Instance}: client pid $($p.Id), account set '$Account'"
    }
}

# A server's first run (no FLServer.cfg) shows its configuration dialog; -PvP checks "Allow Players to Harm Other
# Players" there. The flag lives in the binary FLServer.cfg: a server already configured the other way is reconfigured
# by moving its FLServer.cfg aside (kept as FLServer.cfg.<time>) before the start.
function Reset-FLMpServerConfig {
    $cfg = Join-Path (Get-FLMpRoot) 'server\Accts\MultiPlayer\FLServer.cfg'
    if (Test-Path -LiteralPath $cfg) { Move-Item -LiteralPath $cfg "$cfg.$(Get-Date -Format yyyyMMdd_HHmmss)"; 'FLServer.cfg moved aside: the next start asks again' }
}

function Start-FLMpSession([int]$Server = 21, [int[]]$Clients = @(21, 22), [string[]]$Characters = @(), [switch]$PvP) {
    Invoke-FLMpAt $Server {
        if ($PvP) { $script:FLMpPvP = $true }
        try { Start-FLMpServer } finally { $script:FLMpPvP = $false }
    }
    $i = 0
    foreach ($c in $Clients) { Start-FLMpClient $c }
    foreach ($c in $Clients) {
        $name = if ($i -lt $Characters.Count) { $Characters[$i] } else { "FLvTest$c" }
        $i++
        Invoke-FLMpAt $c { "instance ${c}: $(Connect-FLMpServer -Character $name)" }
    }
}

# The client leaves the server as a player does, so the server frees the account at once (a killed client keeps it
# "in use" until the server's timeout): the multiplayer game menu (pause; HDE 0.7.1: RESUME, SELECT NEW CHARACTER,
# SELECT NEW SERVER, OPTIONS, QUIT TO MAIN MENU = PauseButtonBox4, FLVIBED) -> the main menu -> EXIT; the process ends.
function Exit-FLMpClient([int]$TimeoutSec = 60) {
    $p = Get-FLGameProcess
    if (-not $p) { return 'no client' }
    # one step per poll, whatever screen it is on: the game asks first (NN_Confirm: Confirm_delBtnYes); a server that
    # went away shows its error box (NN_Error: ErrorOkayButton -> the server list, whose MAIN MENU leads on); the death
    # dialog's NEW SERVER (NewServerButtonButton) leads to the server list too; in flight or docked the game menu opens
    $null = Wait-FLUntil {
        $u = Send-FL 'uiitems'
        if ($u -match '(?m)^FrontButton') { return $true }
        foreach ($b in 'Confirm_delBtnYes', 'ErrorOkayButton', 'MainMenu', 'NewServerButtonButton', 'PauseButtonBox4') {
            if ($u -match "(?m)^$b\s" -and (Send-FL "visible $b") -match '^visible') { Send-FL "click $b" | Out-Null; return $false }
        }
        if ((Send-FL 'state') -match 'mode=(space|docked)') { Send-FL 'pause' | Out-Null }
        $false } 'the main menu' 45
    Send-FL 'menu exit' | Out-Null
    # EXIT asks too (NN_Exit: ExitButtonBox0 = yes, ExitButtonBox1 = no)
    $null = Wait-FLUntil { if ((Send-FL 'visible ExitButtonBox0') -match '^visible') { Send-FL 'click ExitButtonBox0' | Out-Null; $true } else { $p.HasExited } } 'the exit prompt' 15
    if (-not $p.WaitForExit($TimeoutSec * 1000)) { throw "client pid $($p.Id) still running $TimeoutSec s after EXIT" }
    "client pid $($p.Id) left the server and exited ($($p.ExitCode))"
}

function Stop-FLMpSession([int]$Server = 21, [int[]]$Clients = @(21, 22)) {
    foreach ($c in $Clients) {
        Invoke-FLMpAt $c { if (Get-FLGameProcess) { try { "instance ${c}: $(Exit-FLMpClient)" } catch { "instance ${c}: quit failed ($_)" } } }
    }
    Invoke-FLMpAt $Server { Stop-FLMpServer }
}

function Get-FLMpAccounts {
    $root = Join-Path (Get-FLMpRoot) 'accounts'
    $sets = if (Test-Path -LiteralPath $root) { Get-ChildItem -LiteralPath $root -Directory | ForEach-Object { "$($_.Name) ($((Get-ChildItem $_.FullName).Count) values, $($_.LastWriteTime))" } } else { @() }
    $last = Get-Content -LiteralPath (Join-Path (Get-FLMpRoot) 'mptest.log') -ErrorAction SilentlyContinue | Where-Object { $_ -match "account set" } | Select-Object -Last 1
    [pscustomobject]@{ Sets = $sets; LastUse = $last }
}
