# The HUD's module rows in game (the module split plan's P9, private): Controller without the HUD, Subtitles without it, the HUD without the Controller. Background games,
# frames in research\experience\runs\hud_modules. Prints what it read; exit 1 on a failure.
# Row classic: the HUD module on with hud_style 0 (the classic HUD selected), in flight and docked: the door's count of the calls into the HUD module
# (core\ui\hud_door.h, debug huddoor) and a style switch 2, 0, 2. The classic HUD runs no HUD 2 code (audit R4c-1): any call fails (-Strict, the
# switch of the time before, is kept and changes nothing).
# Row nnpad: the controller without the HUD in flight: RB pressed while a NeuroNet window slides open, the window closed with View, then the right
# stick must turn the ship and no press stays held (`nnwin`; audit 65e F1: the held press was never sent and the pad's flight stood down for good).
# Row measure: HUD 2 in flight, both forms, hud_scale 80 and 130: the kit's text size when HUD 2's layout measures is the size its pass draws with
# (`hud2 state` fonts; audit 65e F4: the layout measured with the size the last other module's pass left, 21 px against 19-25 at 1280x800).
# Row dealer: HUD 2's commodity page standing, hud_style 2 -> 0: the game's window must close at the switch and the dealer reopen classic on the screen (audit
# 65e F3: the page's window stood off the screen, undrawn, until closed).
# Row reset: the controller without the HUD, docked: the options page drawn with the shared kit, then a device Reset: the game's own Reset after
# it must succeed (audit 65e F2: the kit's state block lived, deleted only through HUD 2, and every Reset failed).
# Row idle: the idle cursor's hide with the keyboard and mouse in flight on the classic HUD and on HUD 2: hidden after 3 s still, shown at once and
# never hidden over a NeuroNet window (a mouse move: hudwake_test; a background game takes none) (`idlecursor`; the owner's decision 62: it works on the classic HUD too); then idle_cursor 0 on the classic HUD: never hidden.
# Row loadswitch: hud_style 2 -> 0 inside a load (`huddoor loadstyle`, at its 3rd frame): HUD 2 drew only the frames before, the game the rest, the
# base classic with no call into HUD 2; then 0 -> 2 inside a load: the game's screen to its end, HUD 2 after (the owner's decision 63). Frames of
# both loads for the run-judge (no HUD 2 element after the switch).
# Row hooks: HUD 2's two hooks (the loading screen, the target label's mute) installed at its first selection, never before (audit R4c-2):
# a cold hud_style 0 start (deploy -Set) lists neither in `hooks`, its load ran unhooked and no call reaches HUD 2 over 120 frames (the
# cold-start row of R4c-3); hud_style 2 lists both; back to 0 they
# stay and a load is the game's own in every frame; the same cold start and a default one with the controller off (core's page host).
# Row reload (asked by name only: the DEV build, build\dev, since a release never frees its logic): a hot reload with HUD 2's INFO page up, with
# a dealer page up and with the job board up: after it HUD 2's page again or the game's window on the screen; on the classic HUD the game's
# window on the screen, and the job board's single controls (off the screen under HUD 2) rebuilt on it (audit R4c-3).
# Usage (PowerShell tool): tests\hud_modules.ps1 [-Row pad,nnpad,measure,dealer,reset,subs,hud,idle,loadswitch,hooks,classic | reload] [-Instance 1] [-Strict]    build\release (reload: build\dev); an unknown row name is refused
param([string[]]$Row = @('pad', 'nnpad', 'measure', 'dealer', 'reset', 'subs', 'hud', 'idle', 'loadswitch', 'hooks', 'classic'), [string]$Instance = 1, [switch]$Strict)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
# A row name this script does not run is refused before any game starts (13 Nov 2026: `-Row nosuch` ran nothing and said PASS). The known names
# are read from the script's own row tests below (`if ('<name>' -in $Row)`), so a row added there is known here without a second list.
$known = @([regex]::Matches((Get-Content -LiteralPath $PSCommandPath -Raw), "(?m)^\s*if \('([a-z0-9_]+)' -in \`$Row\)") | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
$unknown = @($Row | Where-Object { $_ -notin $known })   # (case-blind, as the row tests' -in)
if (-not $known.Count -or $unknown.Count -or -not $Row.Count) { throw "hud_modules: unknown row(s) '$($unknown -join ', ')' - the rows are: $($known -join ', ')" }
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$deploy = Join-Path $root 'tools\deploy.ps1'
$dir = Join-Path $root 'research\experience\runs\hud_modules'
$bad = [Collections.Generic.List[string]]::new()
$crashSince = Get-Date   # (a crash recorder block written after this is the run's: Get-FLCrashBlocks at the end)
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Stop-Game { Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue; Get-FLGameProcess | Wait-Process -Timeout 20 -ErrorAction SilentlyContinue }
function Wait-Sec([double]$s) { Start-Sleep -Milliseconds ([int]($s * 1000)) }
function Head([string]$s, [int]$n = 150) { "$s".Substring(0, [Math]::Min($n, "$s".Length)) }
function No-Fault([string]$log) { Check ((Get-Content (Join-Path (Get-FLInstallDir) "EXE\$log") -Raw) -notmatch 'FATAL') "no fault in $log" }

try {
    if ('pad' -in $Row) {
        '--- the controller without the HUD: the classic HUD, the pad prompts'
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,hud=0' -Save SaveC701.fl | Select-Object -Last 1
        Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad; $null = Wait-FLFramesPassed 30
        Check ("$(Send-FL 'hud2 state')" -notmatch 'style') "hud2: no verb ($(Head (Send-FL 'hud2 state') 60))"
        $st = Send-FL 'state'
        "  $(Head $st 110)"
        Save-FLShot -Name 'pad_docked' -Dir $dir
        $prompt = Send-FL 'prompttrace'
        "  prompts: $(Head $prompt 200)"
        Check ((Send-FL 'ping') -eq 'pong') 'the game answers'
        No-Fault 'FLController.log'
        Stop-Game
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,hud=0' -Save SaveC702.fl | Select-Object -Last 1
        Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad; $null = Wait-FLFramesPassed 30
        Check ("$(Send-FL 'padhud')" -match 'active=1') 'the pad HUD layer is up'
        "  padhud: $(Head (Send-FL 'padhud') 200)"
        Save-FLShot -Name 'pad_flight' -Dir $dir
        No-Fault 'FLController.log'
        Stop-Game
    }
    if ('nnpad' -in $Row) {
        '--- the controller without the HUD: a bumper in a NeuroNet slide in flight, then the ship still steers (audit 65e F1)'
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,hud=0' -Save SaveC706.fl | Select-Object -Last 1
        $null = Send-FL 'set input_mode 1'
        # RB right after the game's command, while the inventory slides in and the mode still reads flight: a press is held (STATUS). If the
        # bumper missed the slide (no hold), the window is closed and the try made again
        $after = ''
        for ($try = 1; $try -le 3; $try++) {
            $null = Send-FL 'tap USER_INVENTORY'
            Press-FLPad RB
            $after = "$(Send-FL 'nnwin')"
            "  try $try, after RB: $after"
            if ($after -match 'held=0x65') { break }
            for ($i = 0; $i -lt 60 -and "$(Send-FL 'nnwin')" -notmatch 'open=0x66 held=0x0 busy=0'; $i++) { $null = Wait-FLFramesPassed 10 }
            $null = Send-FL 'tap USER_INVENTORY'
            for ($i = 0; $i -lt 60 -and "$(Send-FL 'nnwin')" -notmatch 'open=0x0 held=0x0 busy=0'; $i++) { $null = Wait-FLFramesPassed 10 }
        }
        Check ($after -match 'held=0x65') 'RB in the slide holds a press (the case under test)'
        # the hold is sent once the slide ends: STATUS stands, nothing held (about 1-2 s)
        for ($i = 0; $i -lt 60 -and "$(Send-FL 'nnwin')" -notmatch 'open=0x65 held=0x0 busy=0'; $i++) { $null = Wait-FLFramesPassed 10 }
        $nn = "$(Send-FL 'nnwin')"
        Check ($nn -match 'open=0x65 held=0x0 busy=0') "the stepped tab (STATUS) opened: $nn"
        # View closes the window (Escape) once the mode reads the window (space-ui): the manager stands settled a few frames before, and View there is
        # flight's (seen 12 Nov 2026, audit 79: 1-2 in 10 with no wait, also on a window opened straight away); up to three presses
        for ($i = 0; $i -lt 60 -and "$(Send-FL 'state')" -notmatch 'mode=space-ui'; $i++) { $null = Wait-FLFramesPassed 5 }
        for ($k = 0; $k -lt 3 -and "$(Send-FL 'nnwin')" -notmatch 'open=0x0 '; $k++) {
            Press-FLPad Back
            for ($i = 0; $i -lt 120 -and "$(Send-FL 'nnwin')" -notmatch 'open=0x0 '; $i += 5) { $null = Wait-FLFramesPassed 5 }
        }
        Check ("$(Send-FL 'nnwin')" -match 'open=0x0 ') "View closed the window ($k presses)"
        for ($i = 0; $i -lt 300 -and "$(Send-FL 'state')" -notmatch 'mode=space\b(?!-)'; $i += 5) { $null = Wait-FLFramesPassed 5 }
        $null = Wait-FLFramesPassed 150 # (past the hold's 2.45 s at the most)
        $nn = "$(Send-FL 'nnwin')"
        "  closed: $nn"
        Check ($nn -match 'held=0x0\b') 'no NeuroNet press held after the window closed'
        Set-FLPad -RX 0.8; $null = Wait-FLFramesPassed 45
        $ship = "$(Send-FL 'ship')"; Clear-FLPad
        $yaw = if ($ship -match 'y\(yaw\)=(-?[\d.]+)') { [double]$Matches[1] } else { 0 }
        Check ([Math]::Abs($yaw) -gt 5) "the right stick turns the ship (yaw $yaw deg/s)"
        Save-FLShot -Name 'nnpad_flight' -Dir $dir
        No-Fault 'FLController.log'
        Stop-Game
    }
    if ('measure' -in $Row) {
        '--- HUD 2 measures its layout with its own text size (audit F4)'
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,hud=1' -Save SaveC706.fl | Select-Object -Last 1
        $null = Send-FL 'set hud_fade 0'
        try {
            foreach ($mode in 1, 2) {
                $null = Send-FL "set input_mode $mode"
                foreach ($scale in 80, 130) {
                    $null = Send-FL "set hud_scale $scale"
                    $null = Send-FL 'hud2 sim combat'
                    $null = Wait-FLFramesPassed 30
                    $f = ([regex]::Match("$(Send-FL 'hud2 state')", 'fonts measured=(\d+)px drawn=(\d+)px')).Groups
                    $form = if ($mode -eq 1) { 'pad' } else { 'K&M' }
                    Check ($f[1].Success -and [int]$f[2].Value -gt 0 -and $f[1].Value -eq $f[2].Value) "$form, hud_scale ${scale}: measured $($f[1].Value) px, drawn $($f[2].Value) px"
                }
            }
            Save-FLShot -Name 'measure_k130' -Dir $dir
        } finally { $null = Send-FL 'hud2 sim off'; $null = Send-FL 'set hud_scale 100' }
        No-Fault 'FLController.log'
        Stop-Game
    }
    if ('dealer' -in $Row) {
        '--- HUD 2 left over a dealer page: hud_style 2 -> 0 with the commodity page standing, then the classic dealer (audit 65e F3)'
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,hud=1' -Save SaveC701.fl | Select-Object -Last 1
        $null = Send-FL 'set hud_style 2'
        Wait-FLRoomReady
        if ((Send-FL 'hotspots') -notmatch '(?m)^IDS_HOTSPOT_COMMODITYTRADER\s') { $null = Goto-FLRoom 'Trader'; Wait-FLRoomReady }
        $null = Wait-FLUntil { $x = Send-FL 'goto IDS_HOTSPOT_COMMODITYTRADER'; if ($x -match '^clicked') { $x } } 'the commodity trader' 15
        $null = Wait-FLUntil { if ((Send-FL 'basepage') -match '^basepage kind=1 ') { 'up' } } 'the commodity page' 20
        $null = Wait-FLFramesPassed 60
        "  page: $(Head (Send-FL 'basepage') 100)"
        $seq = [int]([regex]::Match((Send-FL 'events 999999999'), '^last (\d+)').Groups[1].Value)
        $null = Send-FL 'set hud_style 0'
        $gone = try { Wait-FLUntil { if ((Send-FL 'visible NN_Dealer') -notmatch '^visible' -and (Send-FL 'basepage') -match '^basepage kind=0 ') { 'closed' } } 'the dealer closed at the switch' 10 } catch { "$_" }
        Check ($gone -eq 'closed') "the switch closed the page's window ($(Head (Send-FL 'basepage') 60); $(Head (Send-FL 'ctlrect NN_Inventory') 80))"
        # windowtext gives the window's rows back while they stand, THEN the close: a close in the same frame first left it writing into the closed window's
        # freed controls (audit 95_: a /GS fail-fast at the ship dealer's final stage, 0xC0000409)
        $ev = @((Send-FL "events $seq") -split "`n" | Select-Object -Skip 1)
        $iw = -1; $ic = -1
        for ($i = 0; $i -lt $ev.Count; $i++) { if ($iw -lt 0 -and $ev[$i] -match 'windowtext off:') { $iw = $i }; if ($ic -lt 0 -and $ev[$i] -match 'basepage: HUD 2 left') { $ic = $i } }
        Check ($iw -ge 0 -and $ic -ge 0 -and $iw -lt $ic) "windowtext stood down before the page's close was pressed (event #$iw, close #$ic)"
        if ($gone -ne 'closed') { Save-FLShot -Name 'dealer_switch_failed' -Dir $dir; $null = Send-FL 'key 0x1B'; $null = Wait-FLFramesPassed 90 }
        $null = Wait-FLUntil { $x = Send-FL 'goto IDS_HOTSPOT_COMMODITYTRADER'; if ($x -match '^clicked') { $x } } 'the commodity trader again' 15
        $null = Wait-FLUntil { if ((Send-FL 'visible NN_Inventory') -match '^visible') { 'up' } } 'the classic dealer' 15
        $null = Wait-FLFramesPassed 60
        $r = "$(Send-FL 'ctlrect NN_Inventory')"
        "  classic dealer: $(Head $r 120) | $(Head (Send-FL 'basepage') 60)"
        $left = if ($r -match 'left=(-?[\d.]+)') { [double]$Matches[1] } else { 1e9 }
        Check ($left -lt 3840 -and (Send-FL 'basepage') -match '^basepage kind=0 ') 'the classic dealer stands on the screen, no page'
        Save-FLShot -Name 'dealer_classic' -Dir $dir
        No-Fault 'FLController.log'
        Stop-Game
    }
    if ('reset' -in $Row) {
        '--- the controller without the HUD: a device Reset after the draw kit drew (audit 65e F2)'
        # The options page draws with the kit; then a Reset. A background game's own `d3dreset` answers INVALIDCALL healthy or not, so the check is
        # the game's own Reset a second later: D3D_OK when nothing was left alive, INVALIDCALL every second (the screen frozen) when the kit's
        # state block was (before the fix)
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,hud=0' -Save SaveC701.fl | Select-Object -Last 1
        $log = Join-Path (Get-FLInstallDir) 'EXE\FLController.log'
        if (Select-String -LiteralPath $log -Pattern "Direct3D 8: the system's" -Quiet) { "  skipped: the system's Direct3D 8 (no wrapper to refuse a Reset)" }
        else {
            Check ("$(Send-FL 'ui options open')" -eq 'open') 'the options page opened (the kit draws)'
            $null = Wait-FLFramesPassed 30
            "  $(Send-FL 'd3dreset')"
            Wait-Sec 3
            $resets = @(Select-String -LiteralPath $log -Pattern 'D3D8 device Reset -> (0x[0-9A-F]+)' | ForEach-Object { $_.Matches[0].Groups[1].Value })
            $last = if ($resets.Count) { $resets[-1] } else { 'none' }
            Check ($last -eq '0x00000000') "the game's own Reset after it succeeds ($last; $($resets.Count) Resets)"
            $null = Send-FL 'ui options close'
            Check ((Send-FL 'ping') -eq 'pong') 'the game answers'
        }
        Save-FLShot -Name 'reset_h0' -Dir $dir
        No-Fault 'FLController.log'
        Stop-Game
    }
    if ('subs' -in $Row) {
        '--- the subtitles without the HUD: the classic HUD, keep-outs through hud_bus'
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles' -Save SaveC702.fl | Select-Object -Last 1
        Wait-Sec 3
        Check ((Send-FLS 'ping') -eq 'pong') 'the subtitles answer'
        $hooks = @((Send-FLS 'hooks') -split "`n" | ForEach-Object { ($_ -split ' ')[0] })
        Check (($hooks -notcontains 'HandleKey') -and ($hooks -notcontains 'UiControlAdd')) 'no HUD or pad hooks'
        Save-FLShot -Name 'subs_flight' -Dir $dir
        No-Fault 'FLSubtitles.log'
        Stop-Game
    }
    if ('hud' -in $Row) {
        '--- the HUD without the controller: HUD 2 on keyboard and mouse'
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,hud=1' -Save SaveC702.fl | Select-Object -Last 1
        Wait-Sec 3
        Check ("$(Send-FLS 'mode')" -eq 'space') "mode: $(Send-FLS 'mode')"
        $h = Send-FLS 'hud2 state'
        "  $(Head $h 220)"
        Check ("$h" -match 'on=1|active=1|shown=1|style=2') 'HUD 2 runs'
        $hooks = @((Send-FLS 'hooks') -split "`n" | ForEach-Object { ($_ -split ' ')[0] })
        Check (($hooks -contains 'HandleKey') -and ($hooks -contains 'UiControlAdd')) 'its shared hooks'
        Save-FLShot -Name 'hud_flight' -Dir $dir
        "  uiskin: $(Head (Send-FLS 'uiskin') 160)"
        "  cue: $(Head (Send-FLS 'cue') 120)"
        Check ((Send-FLS 'ping') -eq 'pong') 'the game answers'
        No-Fault 'FLSubtitles.log'
        Stop-Game
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,hud=1' -Save SaveC701.fl | Select-Object -Last 1
        Wait-Sec 3
        "  docked uiskin: $(Head (Send-FLS 'uiskin') 160)"
        Save-FLShot -Name 'hud_docked' -Dir $dir
        No-Fault 'FLSubtitles.log'
        Stop-Game
    }
    if ('idle' -in $Row) {
        '--- the idle cursor, keyboard and mouse in flight, on both interfaces: 3 s still -> hidden, a NeuroNet window -> shown, never hidden (decision 62)'
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,hud=1' -Save SaveC706.fl | Select-Object -Last 1
        $null = Send-FL 'set input_mode 2'; $null = Send-FL 'set hud_fade 1'
        try {
            foreach ($s in 0, 2) {
                $null = Send-FL "set hud_style $s"
                # (a background game takes no mouse motion: the harness's `mouse` never moves its cursor, so "a move shows it" is hudwake_test's;
                # here the window: every state but free flight shows it at once)
                $hid = try { Wait-FLUntil { if ((Send-FL 'idlecursor') -match 'hidden=1') { 'hidden' } } 'the cursor idle-hidden' 10 } catch { "$_" }
                Check ($hid -eq 'hidden') "hud_style ${s}: hidden after 3 s still ($(Send-FL 'idlecursor'))"
                Save-FLShot -Name "idle_hidden_$s" -Dir $dir
                $null = Send-FL 'tap USER_NAV_MAP'
                for ($i = 0; $i -lt 60 -and "$(Send-FL 'state')" -notmatch 'mode=space-ui'; $i++) { $null = Wait-FLFramesPassed 5 }
                Check ((Send-FL 'idlecursor') -match 'hidden=0') "hud_style ${s}: shown again at once over a NeuroNet window ($(Send-FL 'idlecursor'))"
                Start-Sleep -Seconds 4 # (the rest is 3 s of real time)
                Check ((Send-FL 'idlecursor') -match 'hidden=0') "hud_style ${s}: never hidden over a NeuroNet window ($(Send-FL 'idlecursor'))"
                Save-FLShot -Name "idle_nn_$s" -Dir $dir
                $null = Send-FL 'tap USER_NAV_MAP'
                for ($i = 0; $i -lt 60 -and "$(Send-FL 'state')" -notmatch 'mode=space\b(?!-)'; $i++) { $null = Wait-FLFramesPassed 5 }
            }
            # the idle_cursor row (CUES AND TEXT, 14 Nov 2026) turns the hide off on the classic HUD, whatever hud_fade says
            $null = Send-FL 'set hud_style 0'; $null = Send-FL 'set idle_cursor 0'
            Start-Sleep -Seconds 4 # (the rest is 3 s of real time)
            Check ((Send-FL 'idlecursor') -match 'rule=classic eligible=1 hidden=0 .*idle_cursor=0') "hud_style 0, idle_cursor 0: never hidden in free flight ($(Send-FL 'idlecursor'))"
            $null = Send-FL 'set idle_cursor 1'
        } finally { $null = Send-FL 'set input_mode 0' }
        No-Fault 'FLController.log'
        Stop-Game
    }
    if ('loadswitch' -in $Row) {
        '--- a load during the switch: hud_style 2 -> 0 at the load''s 3rd frame, the rest of the load and the base classic; then 0 -> 2 in a load (decision 63)'
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,hud=1' -Save SaveC706.fl | Select-Object -Last 1
        $null = Send-FL 'set hud_style 2'; $null = Wait-FLFramesPassed 10
        $null = Send-FL 'huddoor reset'
        $null = Send-FL 'huddoor loadstyle 0'
        # the game's own load from the pause menu (Load-FLSave's steps), frames of the load screen taken from outside the game (no command reaches a load)
        $null = Wait-FLUntil { if ((Send-FL 'pause') -match 'open') { 'open' } } 'pause menu' 10
        $null = Wait-FLUntil { if ((Send-FL 'pausecodes') -notmatch 'not open') { 'up' } } 'pause menu' 10
        $null = Send-FL 'pauseload'
        $null = Wait-FLUntil { if ((Send-FL 'saves') -notmatch 'no load screen open') { 'list' } } 'load screen' 15
        $null = Send-FL 'loadsave SaveC701.fl'
        for ($i = 0; $i -lt 24; $i++) { Save-FLShot -Name ("loadswitch_{0:D2}" -f $i) -Dir $dir | Out-Null; Start-Sleep -Milliseconds 250 }
        $null = Wait-FLUntil { if ((Send-FL 'state') -match 'mode=docked') { 'docked' } } 'docked after the load' 60
        Wait-FLRoomReady
        $d = "$(Send-FL 'huddoor')"
        "  $($d -replace "`n", ' | ')"
        $hud2 = if ($d -match 'load frames hud2=(\d+) game=(\d+) refused=(\d+)') { [int]$Matches[1] } else { -1 }
        $game = if ($d -match 'load frames hud2=\d+ game=(\d+)') { [int]$Matches[1] } else { -1 }
        Check ($hud2 -eq 2) "HUD 2 drew the load's two frames before the switch, none after ($hud2)"
        Check ($game -ge 1) "the game's own screen drew the rest of the load ($game frames)"
        Check ($d -match 'selected=classic' -and $d -match 'views=0' -and $d -match 'leaves=1') 'the base arrives on the classic HUD, HUD 2 left once'
        $null = Send-FL 'huddoor reset'; $null = Wait-FLFramesPassed 120
        Check ("$(Send-FL 'huddoor')" -match 'hud2_calls=0 ') "no call into HUD 2 after the load ($(Head (Send-FL 'huddoor') 160))"
        Save-FLShot -Name 'loadswitch_base' -Dir $dir
        # the other way: hud_style 0 -> 2 inside a load: the game's screen to its end (HUD 2 enters after it)
        $null = Send-FL 'huddoor loadstyle 2'
        $null = Wait-FLUntil { if ((Send-FL 'pause') -match 'open') { 'open' } } 'pause menu' 10
        $null = Wait-FLUntil { if ((Send-FL 'pausecodes') -notmatch 'not open') { 'up' } } 'pause menu' 10
        $null = Send-FL 'pauseload'
        $null = Wait-FLUntil { if ((Send-FL 'saves') -notmatch 'no load screen open') { 'list' } } 'load screen' 15
        $null = Send-FL 'loadsave SaveC706.fl'
        for ($i = 0; $i -lt 12; $i++) { Save-FLShot -Name ("loadswitch_back_{0:D2}" -f $i) -Dir $dir | Out-Null; Start-Sleep -Milliseconds 250 }
        $null = Wait-FLUntil { if ((Send-FL 'state') -match 'mode=space\b(?!-)') { 'space' } } 'in space after the load' 60
        $null = Wait-FLFramesPassed 30
        $d = "$(Send-FL 'huddoor')"
        "  $($d -replace "`n", ' | ')"
        Check ($d -match 'load frames hud2=0 ' -and $d -match 'selected=hud2' -and $d -match 'hud_style=2') 'a switch to HUD 2 in a load: the game''s screen to its end, HUD 2 after it'
        No-Fault 'FLController.log'
        Stop-Game
    }
    if ('hooks' -in $Row) {
        '--- HUD 2''s two hooks (the loading screen, the target label''s mute) installed at its first selection, never before (audit R4c-2)'
        # (the hook rows' names: core\flexe.h UiBatchClose, HudVertex, HudIndicatorDraw)
        function Hud2Hooks([string]$h) { @(@('UiBatchClose', 'HudVertex', 'HudIndicatorDraw') | Where-Object { $h -match "(?m)^$_ " }) }
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,hud=1' -Save SaveC706.fl -Set 'hud_style=0' | Select-Object -Last 1
        $null = Wait-FLFramesPassed 30
        $d = "$(Send-FL 'huddoor')"
        "  cold hud_style 0: $($d -replace "`n", ' | ')"
        Check ($d -match 'selected=classic' -and $d -match 'hud_style=0 enters=0 ') 'a cold hud_style 0 start: HUD 2 never selected'
        Check ($d -match 'load frames hud2=0 game=0 ') 'the save''s load ran unhooked (the door saw no load frame)'
        $h = Hud2Hooks "$(Send-FL 'hooks')"
        Check ($h.Count -eq 0) "a cold hud_style 0 start: none of HUD 2's hooks installed ($($h -join ', '))"
        # the cold-start count (audit R4c-3): the classic HUD from the first frame, no call into HUD 2 (the classic row starts on 2 and switches)
        $null = Send-FL 'huddoor reset'; $null = Wait-FLFramesPassed 120
        $d = "$(Send-FL 'huddoor')"
        $calls = if ($d -match 'hud2_calls=(\d+)') { [int64]$Matches[1] } else { -1 }
        $frames = if ($d -match 'classic frames=(\d+)') { [int]$Matches[1] } else { 0 }
        Check ($calls -eq 0 -and $frames -ge 100) "a cold hud_style 0 start: no call into HUD 2 ($calls over $frames classic frames)"
        Save-FLShot -Name 'hooks_cold0_flight' -Dir $dir
        $null = Send-FL 'set hud_style 2'; $null = Wait-FLFramesPassed 30
        $h = Hud2Hooks "$(Send-FL 'hooks')"
        Check ("$(Send-FL 'huddoor')" -match 'selected=hud2' -and $h.Count -eq 3) "hud_style 2: HUD 2 entered, its hooks installed ($($h -join ', '))"
        $null = Send-FL 'set hud_style 0'; $null = Wait-FLFramesPassed 30
        $h = Hud2Hooks "$(Send-FL 'hooks')"
        Check ("$(Send-FL 'huddoor')" -match 'selected=classic' -and $h.Count -eq 3) "back to hud_style 0: the hooks stay, passing through ($($h -join ', '))"
        # a load on the classic HUD with the hook in: the game's own screen in every load frame
        $null = Send-FL 'huddoor reset'
        $null = Wait-FLUntil { if ((Send-FL 'pause') -match 'open') { 'open' } } 'pause menu' 10
        $null = Wait-FLUntil { if ((Send-FL 'pausecodes') -notmatch 'not open') { 'up' } } 'pause menu' 10
        $null = Send-FL 'pauseload'
        $null = Wait-FLUntil { if ((Send-FL 'saves') -notmatch 'no load screen open') { 'list' } } 'load screen' 15
        $null = Send-FL 'loadsave SaveC701.fl'
        for ($i = 0; $i -lt 8; $i++) { Save-FLShot -Name ("hooks_load0_{0:D2}" -f $i) -Dir $dir | Out-Null; Start-Sleep -Milliseconds 250 }
        $null = Wait-FLUntil { if ((Send-FL 'state') -match 'mode=docked') { 'docked' } } 'docked after the load' 60
        Wait-FLRoomReady
        $d = "$(Send-FL 'huddoor')"
        "  after the classic load: $($d -replace "`n", ' | ')"
        $game = if ($d -match 'load frames hud2=0 game=(\d+)') { [int]$Matches[1] } else { -1 }
        Check ($game -ge 1) "a load on the classic HUD after HUD 2 was selected: the game's own screen in all of its $game frames, none of HUD 2's"
        Check ($d -match 'hud2_calls=0 ') "no call into HUD 2 on the classic HUD with its hooks in ($(Head $d 160))"
        Save-FLShot -Name 'hooks_classic_docked' -Dir $dir
        No-Fault 'FLController.log'
        Stop-Game
        # the controller off (core's page host installs from its own host table): a cold hud_style 0 start, then one at the default 2
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,hud=1' -Save SaveC706.fl -Set 'hud_style=0' | Select-Object -Last 1
        Wait-Sec 3
        $st = "$(Send-FLS 'hud2 state')"
        $h = Hud2Hooks "$(Send-FLS 'hooks')"
        Check ($st -match 'settings style=0' -and $st -match 'hud2 active=0') "the controller off, a cold hud_style 0 start: the classic HUD ($(Head $st 90))"
        Check ($h.Count -eq 0) "the controller off, cold hud_style 0: none of HUD 2's hooks installed ($($h -join ', '))"
        Save-FLShot -Name 'hooks_nocontroller_cold0' -Dir $dir
        No-Fault 'FLSubtitles.log'
        Stop-Game
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,hud=1' -Save SaveC706.fl | Select-Object -Last 1
        Wait-Sec 3
        $h = Hud2Hooks "$(Send-FLS 'hooks')"
        Check ("$(Send-FLS 'hud2 state')" -match 'hud2 active=1' -and $h.Count -eq 3) "the controller off, hud_style 2: HUD 2 up, its hooks installed ($($h -join ', '))"
        No-Fault 'FLSubtitles.log'
        Stop-Game
    }
    if ('reload' -in $Row) {
        '--- a hot reload with HUD 2''s pages up (DEV build: the release build has no reload): INFO, the dealer, the job board (audit R4c-3)'
        # INFO: the old logic's stop gives the page runtime's park back (pagerun::Shutdown) and the new logic takes the window again; on the
        # classic HUD the game's own window stands on the screen. The dealer: the new logic takes the page again (what basepage's Away left is
        # not seen, since the page is taken again), and the classic dealer reopens on the screen. The job board: HUD 2 moves its single
        # controls a screen's width off (PlaceAt -W, basepage.cpp); after the reload and on the classic HUD the game rebuilds them on the screen
        # when the board reopens (65e's open question). A control's centre by `ctlrect` (any class, on the screen or not; `uiitems` lists
        # only what stands on the screen), waited for up to 5 s (a window's slide-in; the dealer's inventory control reaches past the
        # screen's left edge even on a plain classic start: left=-410 at 1280x800, so its centre is read, not its left)
        function CenterX([string]$name) { $c = "$(Send-FL "ctlrect $name")"; if ($c -match 'center=\((-?[\d.]+|-?nan\S*),') { $Matches[1] } else { '' } }
        function OnScreen([string]$name) {
            $r = try { Wait-FLUntil { $x = CenterX $name; if ($x -match '^-?[\d.]+$' -and [double]$x -ge 0) { $x } } "$name on the screen" 5 } catch { '' }
            [bool]$r
        }
        $jobs = @('MissionVendorTitle', 'MissionVendorAcceptButton', 'MissionVendorCloseButton')
        & $deploy -Background -NoBuild -Instance $Instance -Modules 'subtitles,controller,hud=1' -Save SaveC701.fl | Select-Object -Last 1
        $null = Send-FL 'set hud_style 2'
        Wait-FLRoomReady
        $null = Send-FL 'nnstrip tab 4' # (docked: the strip's INFO word; `go` needs a window open)
        $null = Wait-FLUntil { if ((Send-FL 'nnpage') -match '^nnpage page=4 ') { 'info' } } 'HUD 2''s INFO page' 20
        "  before: $(Head (Send-FL 'page') 90)"
        "  reload: $(Send-FL 'reload' 15000)"
        Check ((Send-FL 'ping') -eq 'pong') 'the game answers after the reload'
        $null = Wait-FLFramesPassed 90
        $p = "$(Send-FL 'nnpage')"; $pg = "$(Send-FL 'page')"
        "  after: $(Head $p 60) | $(Head $pg 90)"
        Check (($p -match '^nnpage page=4 ' -and $pg -match 'parked=1') -or (OnScreen 'NN_Help')) 'INFO after the reload: HUD 2''s page again, or the game''s window on the screen'
        Save-FLShot -Name 'reload_info_2' -Dir $dir
        $null = Send-FL 'set hud_style 0'; $null = Wait-FLFramesPassed 60
        Check (OnScreen 'NN_Help') "INFO on the classic HUD after the reload: the game's window on the screen ($(Head (Send-FL 'ctlrect NN_Help') 90))"
        Save-FLShot -Name 'reload_info_0' -Dir $dir
        $null = Send-FL 'key 0x1B' # (the game's own close of its window)
        $null = try { Wait-FLUntil { if ((Send-FL 'visible NN_Help') -notmatch '^visible') { 'closed' } } 'INFO closed' 10 } catch { "$_" }
        # the dealer
        $null = Send-FL 'set hud_style 2'; $null = Wait-FLFramesPassed 30
        if ((Send-FL 'hotspots') -notmatch '(?m)^IDS_HOTSPOT_COMMODITYTRADER\s') { $null = Goto-FLRoom 'Trader'; Wait-FLRoomReady }
        $null = Wait-FLUntil { $x = Send-FL 'goto IDS_HOTSPOT_COMMODITYTRADER'; if ($x -match '^clicked') { $x } } 'the commodity trader' 15
        $null = Wait-FLUntil { if ((Send-FL 'basepage') -match '^basepage kind=1 ') { 'up' } } 'the commodity page' 20
        $null = Wait-FLFramesPassed 60
        "  reload: $(Send-FL 'reload' 15000)"
        Check ((Send-FL 'ping') -eq 'pong') 'the game answers after the second reload'
        $null = Wait-FLFramesPassed 90
        $b = "$(Send-FL 'basepage')"
        "  after: $(Head $b 90)"
        Check (($b -match '^basepage kind=1 ') -or (OnScreen 'NN_Inventory')) 'the dealer after the reload: HUD 2''s page again, or the game''s window on the screen'
        Save-FLShot -Name 'reload_dealer_2' -Dir $dir
        $null = Send-FL 'set hud_style 0'
        $null = Wait-FLUntil { if ((Send-FL 'visible NN_Dealer') -notmatch '^visible' -and (Send-FL 'basepage') -match '^basepage kind=0 ') { 'closed' } } 'the dealer closed at the switch' 10
        $null = Wait-FLUntil { $x = Send-FL 'goto IDS_HOTSPOT_COMMODITYTRADER'; if ($x -match '^clicked') { $x } } 'the commodity trader again' 15
        $null = Wait-FLUntil { if ((Send-FL 'visible NN_Inventory') -match '^visible') { 'up' } } 'the classic dealer' 15
        $null = Wait-FLFramesPassed 60
        Check (OnScreen 'NN_Inventory') "the classic dealer after the reload: on the screen ($(Head (Send-FL 'ctlrect NN_Inventory') 90))"
        Save-FLShot -Name 'reload_dealer_0' -Dir $dir
        $null = Send-FL 'key 0x1B'
        $null = try { Wait-FLUntil { if ((Send-FL 'visible NN_Inventory') -notmatch '^visible') { 'closed' } } 'the dealer closed' 10 } catch { "$_" }
        # the job board: its single controls off the screen under HUD 2, rebuilt on the screen by the game on the classic HUD after the reload
        $null = Send-FL 'set hud_style 2'; $null = Wait-FLFramesPassed 30
        if ((Send-FL 'hotspots') -notmatch '(?m)^IDS_HOTSPOT_MISSIONVENDOR\s') { $null = Goto-FLRoom 'Bar'; Wait-FLRoomReady }
        $null = Wait-FLUntil { $x = Send-FL 'goto IDS_HOTSPOT_MISSIONVENDOR'; if ($x -match '^clicked') { $x } } 'the job board' 15
        $null = Wait-FLUntil { if ((Send-FL 'basepage') -notmatch '^basepage kind=0 ') { 'up' } } 'HUD 2''s job board page' 20
        $null = Wait-FLFramesPassed 60
        $xs = @($jobs | ForEach-Object { "$_=$(CenterX $_)" })
        Check (@($jobs | Where-Object { $x = CenterX $_; $x -match '^-?[\d.]+$' -and [double]$x -lt 0 }).Count -eq $jobs.Count) "HUD 2's job board: the game's single controls stand off the screen ($($xs -join ' '))"
        "  reload: $(Send-FL 'reload' 15000)"
        Check ((Send-FL 'ping') -eq 'pong') 'the game answers after the third reload'
        $null = Wait-FLFramesPassed 90
        $null = Send-FL 'set hud_style 0'
        $closed = try { Wait-FLUntil { if ((Send-FL 'visible MissionVendorCloseButton') -notmatch '^visible') { 'closed' } } 'the board closed at the switch' 10 } catch { "$_" }
        if ($closed -ne 'closed') { "  the board did not close at the switch: $(Head $closed 120)"; $null = Send-FL 'key 0x1B'; $null = Wait-FLFramesPassed 60 }
        $null = Wait-FLUntil { $x = Send-FL 'goto IDS_HOTSPOT_MISSIONVENDOR'; if ($x -match '^clicked') { $x } } 'the job board again' 15
        $null = Wait-FLUntil { if ((Send-FL 'visible MissionVendorCloseButton') -match '^visible') { 'up' } } 'the classic job board' 15
        $null = Wait-FLFramesPassed 60
        $xs = @($jobs | ForEach-Object { "$_=$(CenterX $_)" })
        Check (@($jobs | Where-Object { OnScreen $_ }).Count -eq $jobs.Count) "the classic job board after the reload: the game rebuilt its controls on the screen ($($xs -join ' '))"
        Save-FLShot -Name 'reload_jobs_0' -Dir $dir
        No-Fault 'FLController.log'
        Stop-Game
    }
    if ('classic' -in $Row) {
        '--- the HUD module on, hud_style 0: the classic HUD selected (the door counts the calls into HUD 2)'
        foreach ($place in @(@{ Save = 'SaveC702.fl'; Name = 'flight' }, @{ Save = 'SaveC701.fl'; Name = 'docked' })) {
            & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,hud=1' -Save $place.Save | Select-Object -Last 1
            Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad; $null = Wait-FLFramesPassed 30
            $null = Send-FL 'set hud_style 0'
            $null = Wait-FLFramesPassed 10
            $null = Send-FL 'huddoor reset'
            $null = Wait-FLFramesPassed 120
            $d = "$(Send-FL 'huddoor')"
            "  $($place.Name): $($d -replace "`n", ' | ')"
            Check ($d -match 'selected=classic') "$($place.Name): the door has the classic HUD selected"
            $calls = if ($d -match 'hud2_calls=(\d+)') { [int64]$Matches[1] } else { -1 }
            $frames = if ($d -match 'classic frames=(\d+)') { [int]$Matches[1] } else { 0 }
            Check ($frames -ge 100) "$($place.Name): the count covers $frames classic frames"
            Check ($calls -eq 0) "$($place.Name): no call into HUD 2 under the classic HUD ($calls over $frames frames)"
            Check ($d -match 'views=0') "$($place.Name): HUD 2's views withdrawn"
            if ($place.Name -eq 'flight') { Check ("$(Send-FL 'padhud')" -match 'active=1') 'flight: the pad HUD layer is up' }
            Save-FLShot -Name "classic_$($place.Name)" -Dir $dir
            # the switch 2, 0, 2: the door follows the setting each frame
            foreach ($s in 2, 0, 2) {
                $null = Send-FL "set hud_style $s"
                $null = Wait-FLFramesPassed 10
                $want = if ($s -eq 0) { 'classic' } else { 'hud2' }
                Check ("$(Send-FL 'huddoor')" -match "selected=$want") "$($place.Name): hud_style $s selects $want"
            }
            Check ((Send-FL 'ping') -eq 'pong') "$($place.Name): the game answers"
            No-Fault 'FLController.log'
            Stop-Game
        }
    }
} finally {
    Stop-Game
    & $deploy -Instance $Instance -NoBuild -NoStart -Release -Modules subtitles,controller | Out-Null # (-Release: a lane has no dev build)
}
foreach ($c in @(Get-FLCrashBlocks $crashSince)) { Check $false "a game crashed during the run (EXE\FLvibed_crash.log):`n$c" }
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'hud modules: PASS'
